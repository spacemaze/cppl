//===------- SemaLevitation.cpp - Semantic Analysis for C++ Levitation ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for C++ Levitation.
//===----------------------------------------------------------------------===//

#include "clang/Sema/Sema.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/ASTMutationListener.h"
#include "clang/AST/NestedNameSpecifier.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Sema/SemaInternal.h"
#include "clang/Sema/Template.h"

#include <iterator>
using namespace clang;
using namespace sema;

//===--------------------------------------------------------------------===//
// Helpers
//

template<typename T>
using ReversedVectorItems = llvm::iterator_range<typename SmallVectorImpl<T>::reverse_iterator>;

template<typename T>
ReversedVectorItems<T> reverse(SmallVectorImpl<T>& Vector) {
  typedef SmallVectorImpl<T> VectorTy;
  return llvm::iterator_range<typename VectorTy::reverse_iterator>(Vector.rbegin(), Vector.rend());
}

static bool isLevitationGlobal(const NestedNameSpecifier *NNS) {

  if (!NNS || !NNS->isDependent())
    return false;

  // TODO levitation: Put some mark on NNS, and then on resulting NestedNameSpec,
  // that this is what we need.
  auto *First = NNS;

  for (auto *Next = First->getPrefix(); Next; Next = Next->getPrefix()) {
    First = Next;
  }

  if (auto *Id = First->getAsIdentifier()) {
    // TODO levitation: "global" keyword.
    return Id->getName() == "global";
  };

  return false;
}

static Decl *getEnclosingDecl(Scope *S) {
  for (;S; S = S->getParent()) {
    if (auto *DC = S->getEntity())
      if (auto *D = dyn_cast<Decl>(DC))
        return D;
  }
  return nullptr;
}

static Decl* getBuildingDeclaration(bool &IsSpecification, Sema* SemaObj) {
  if (!SemaObj->CodeSynthesisContexts.empty()) {
    IsSpecification = true;
    return SemaObj->CodeSynthesisContexts.front().Entity;
  } else {
    // Parser case.
    // Check whether we parse some named decl (which is record though).
    IsSpecification = false;
    return getEnclosingDecl(SemaObj->getCurScope());
  }
}

struct LevitationPackageOutermostDecls {
  NamespaceDecl *NS;
  NamedDecl *OutermostNamedDecl;
  FunctionDecl *OutermostFunction;
};

static LevitationPackageOutermostDecls getOutermostNamespaceAndTopLevelNamedDecl(Decl *D) {
  // Walk up through DC parents chain,
  // pick the first namespace we met (NS),
  // and remember the last declaration we met (ND).
  // pair of NS and ND is the result we need.
  NamedDecl *ND = dyn_cast<NamedDecl>(D);
  FunctionDecl *FD = ND ? dyn_cast<FunctionDecl>(ND) : nullptr;
  NamespaceDecl* NS = nullptr;

  DeclContext *DC = dyn_cast<DeclContext>(D);
  if (!DC)
    DC = D->getDeclContext();

  while (DC && !NS) {
    // First try to cast to NS, and if we fail, try cast to Decl.
    if (DC->isNamespace())
      NS = cast<NamespaceDecl>(DC);
    else {
      if (auto *D = dyn_cast<NamedDecl>(DC)) {
        ND = D;
        if (auto *F = dyn_cast<FunctionDecl>(DC))
          FD = F;
      }
    }
    DC = DC->getLexicalParent();
  }

  return {NS, ND, FD};
}

typedef std::pair<StringRef, SourceRange> ComponentLoc;

void getDependencyComponents(
        SmallVectorImpl<StringRef> &Components,
        const NestedNameSpecifier *NNS) {

  if (auto *PrefixNNS = NNS->getPrefix()) {
    getDependencyComponents(Components, PrefixNNS);
  }

  switch (NNS->getKind()) {
    case NestedNameSpecifier::Identifier:
      // Skip 'global' keyword
      // TODO Levitation: add 'global' keyword.
      if (NNS->getAsIdentifier()->getName() != "global")
        Components.push_back(NNS->getAsIdentifier()->getName());
      return;

    case NestedNameSpecifier::Namespace:
      if (NNS->getAsNamespace()->isAnonymousNamespace())
        llvm_unreachable("Anonymous namespace? As a dependency prefix?");
      Components.push_back(NNS->getAsNamespace()->getName());
      return;

    case NestedNameSpecifier::NamespaceAlias:
      Components.push_back(NNS->getAsNamespaceAlias()->getName());
      return;

    case NestedNameSpecifier::Global:
      llvm_unreachable("Global NNS can't be a dependency prefix."
                       " 'global' keyword should be used instead.");

    case NestedNameSpecifier::Super:
      llvm_unreachable("__super can't be a dependency prefix");

    case NestedNameSpecifier::TypeSpecWithTemplate:
      // Fall through to print the type.
      LLVM_FALLTHROUGH;

    case NestedNameSpecifier::TypeSpec: {
      const Type *T = NNS->getAsType();
      if (const auto *TST = dyn_cast<DependentTemplateSpecializationType>(T)) {
        Components.push_back(TST->getIdentifier()->getName());
        return;
      }

      llvm_unreachable("Rest of types are not supported by dependency components resolver");
    }
  }
}

levitation::PackageDependency computeAutoDependency(
    const NamedDecl *DependentDeclaration,
    const NestedNameSpecifierLoc &Loc,
    const IdentifierInfo *Name) {

  SmallVector<StringRef, 8> NNSComponents;
  getDependencyComponents(NNSComponents, Loc.getNestedNameSpecifier());
  NNSComponents.push_back(Name->getName());

  levitation::PackageDependencyBuilder DependencyBuilder;
  for (const auto &Component : NNSComponents) {
    DependencyBuilder.addComponent(Component);
  }

  DependencyBuilder.addUse(DependentDeclaration, Loc.getSourceRange());

  return std::move(DependencyBuilder.getDependency());
}

void dumpDependency(
    const SourceManager &SourceMgr,
    const NamedDecl *DependentDeclaration,
    const FunctionDecl *DependentFunction,
    const levitation::PackageDependency &Dependency,
    bool IsBuildingSpecification
) {
  auto &out = llvm::errs();
  DependentDeclaration->printQualifiedName(out);
  out << " ";
  DependentDeclaration->getLocation().print(out, SourceMgr);
  out << " depends on ";
  Dependency.print(out);
  out << ",";

  Dependency.getUses().back().Location.print(out, SourceMgr);

  if (DependentFunction) {
    out << " [definition only, for ";
    DependentFunction->printQualifiedName(out);
    out << "]";
  }

  if (IsBuildingSpecification)
    out << " [through its template specification] ";

  out << "\n";
}

// TODO Levitation: provide this method with SourceRange.
//   Easy to get it when you have a DeclarationNameInfo,
//   and didn't manage to get it during template instantiation stage (rebuild).
bool Sema::HandleLevitationPackageDependency(
      const NestedNameSpecifierLoc &Loc,
      const IdentifierInfo *Name
  ) {
  // 0.0. If Loc is not dependent, boil out. It happens when we instantiate
  //      template, and convert dependent typename into regular one.
  // 0.1. Check whether this is a levitation 'global' reference. If not - boil out.
  // 1. Get enclosing namespace, and top-level declaration which belongs
  //    to this namespace.
  // 2. If namespace is not a levitation package, boil out.
  // 3. So far, we support only enums and classes instantiation, if it is something
  //    different, boil out.
  // 4. Register dependency. Two types of dependencies are possible:
  //    * Declaration dependency. Everything which uses dependent class should pull
  //      out this dependency as well.
  //    * Definition-only dependency. Used in one of dependent class methods, which could
  //      be compiled separately. Then it's not necessary to pull it by dependent class users.

  if (!isLevitationGlobal(Loc.getNestedNameSpecifier()))
    return false;

  bool IsBuildingSpecification;
  Decl* BuildingDecl = getBuildingDeclaration(IsBuildingSpecification, this);

  if (!BuildingDecl)
    return false;

  auto Outermosts = getOutermostNamespaceAndTopLevelNamedDecl(BuildingDecl);
  auto *NS = Outermosts.NS;
  auto *OutermostND = Outermosts.OutermostNamedDecl;
  auto *OutermostF = Outermosts.OutermostFunction;

  if (!NS || !NS->isLevitationPackage() || !OutermostND)
    return false;

  auto *TD = dyn_cast<TagDecl>(OutermostND);
  if (!TD)
    return false;

  bool IsBodyDependency =
          OutermostF &&
          true; // TODO levitation: modify GetGVALinkageForFunction and then use this:
                //   Context.GetGVALinkageForFunction(OutermostF) == GVA_StrongExternal;

  auto Dependency = computeAutoDependency(TD, Loc, Name);

  // TODO Levitation: to be implemented as a FrontendOption
  bool DumpDependency = true;
  if (DumpDependency) {
    dumpDependency(
        getSourceManager(),
        TD,
        OutermostF,
        Dependency,
        IsBuildingSpecification
    );
  }

  if (IsBodyDependency)
    LevitationDefinitionDependencies.mergeDependency(std::move(Dependency));
  else
    LevitationDeclarationDependencies.mergeDependency(std::move(Dependency));

  return true;
}

bool Sema::HandleLevitationPackageDependency(
      const NestedNameSpecifierLoc &Loc,
      const DeclarationNameInfo &Name
  ) {
  HandleLevitationPackageDependency(Loc, Name.getName().getAsIdentifierInfo());
  return true;
}

//===--------------------------------------------------------------------===//
// Package Dependent declarations marker
//

class PackageDependentClassesMarker
  : public RecursiveASTVisitor<PackageDependentClassesMarker> {

    typedef RecursiveASTVisitor<PackageDependentClassesMarker> ParentTy;
    typedef std::function<void(NamedDecl *)> MarkActionTy;

    // TODO levitation: It would be good to have a single
    // PackageDependentDecls visitor for marking decls as
    // dependent and for instantiating them.
    class PackageDependentDeclsVisitor : public DeclVisitor<PackageDependentDeclsVisitor> {
      MarkActionTy &MarkAction;

    public:
      PackageDependentDeclsVisitor(MarkActionTy &markAction) :
        MarkAction(markAction) {}

      // This affects regular classes and structs (which are CXXRecordDecl)
      // Also it affects ClassTemplateSpecializationDecl,
      // which is inharited from CXXRecordDecl.
      void VisitCXXRecordDecl(CXXRecordDecl *D) {

        // Ignore ClassTemplateDecl patterns.
        if (D->getDescribedClassTemplate())
          return;

        MarkAction(D);
      }
      void VisitEnumRecordDecl(EnumDecl *D) {
        llvm_unreachable("Support is not implemented.");
      }

      // Methods which ignore package namespace enclosure
      void VisitClassTemplatePartialSpecializationDecl(ClassTemplatePartialSpecializationDecl *D) {
        MarkAction(D);
      }
      void VisitClassTemplateDecl(ClassTemplateDecl *D) {
        MarkAction(D);
      }

      void VisitNamespaceDecl(NamespaceDecl *D) {
        // Do nothing.
      }

      // Unsupported decls fall here.
      // That should never happen, every unsupported case
      // should be handled by parser with proper diagnostics.
      void VisitNamedDecl(NamedDecl *D) {
        llvm_unreachable("Not supported");
      }
    };

    MarkActionTy MarkAction;
    llvm::DenseSet<NamespaceDecl *> PackageNamespaces;

public:

    PackageDependentClassesMarker(MarkActionTy markAction) : MarkAction(markAction) {}

    llvm::DenseSet<NamespaceDecl *> &getPackageNamespaces() {
      return PackageNamespaces;
    }

    bool TraverseNamespaceDecl(NamespaceDecl *NS) {
      // We are interested only in package namespace from main file.
      bool Skip = !NS->isLevitationPackage() ||
                  !belongsToMainFile(NS);

      if (Skip)
        return ParentTy::TraverseNamespaceDecl(NS);

      PackageNamespaces.insert(NS);

      PackageDependentDeclsVisitor DependentDeclsMarker(MarkAction);
      for (auto *D : NS->decls())
         DependentDeclsMarker.Visit(D);

    return true;
  }

private:
  bool belongsToMainFile(Decl *NS) {
    auto &SourceManager = NS->getASTContext().getSourceManager();
    auto NSFileID = SourceManager.getFileID(NS->getLocation());
    return NSFileID == SourceManager.getMainFileID();
  }
};

void Sema::markLevitationPackageDeclsAsPackageDependent() {

  if (getLangOpts().getLevitationBuildStage() != LangOptions::LBSK_BuildAST)
    llvm_unreachable("Package dependent marking allowed on AST build stage only.");

  PackageDependentClassesMarker Marker([this] (NamedDecl *D) {
    LevitationPackageDependentDecls.push_back(D);
  });

  Marker.TraverseDecl(Context.getTranslationUnitDecl());
}

//===--------------------------------------------------------------------===//
// Package classes instantiation
//

bool Sema::isShadowedLevitationDecl(const Decl *D) const {
  assert(getLangOpts().LevitationMode &&
         "This method is supposed to be used by levitation mode only");
  return
    getLangOpts().getLevitationBuildStage() == LangOptions::LBSK_BuildObjectFile &&
    D->isLevitationPackageDependent();
}

NamedDecl *Sema::substLevitationPackageDependentDecl(const NamedDecl *D) {
  // Probably we don't need filtering by build stage.
  // But then it makes no sense only at Build Object stage
  if (getLangOpts().getLevitationBuildStage() == LangOptions::LBSK_BuildObjectFile) {
    if (auto *Found = findLevitationPackageDependentInstantiationFor(D))
      return cast<NamedDecl>(Found);
  }
  return nullptr;
}

void Sema::addLevitationPackageDependentInstatiation(
    const clang::Decl *PackageDependent,
    clang::Decl *Instantiation) {

  auto insertRes = PackageDependentDeclInstantiations.insert({
    PackageDependent, Instantiation
  });

  if (!insertRes.second)
    llvm_unreachable("Package dependent declaration can't be instatiated twice");
}

Decl* Sema::findLevitationPackageDependentInstantiationFor(const Decl* D) {
  auto Found = PackageDependentDeclInstantiations.find(D);
  if (Found != PackageDependentDeclInstantiations.end())
    return Found->second;
  return nullptr;
}

class PackageDeclsInstantiator : public DeclVisitor<PackageDeclsInstantiator, NamedDecl*> {
private:
  Sema *SemaObj;
  ASTContext &Context;

  // Even though it contains only one field,
  // we underline, that this is valid only during visit call
  struct {
    DeclContext *SemanticDC;
  } VisitContext;

  // Map of declaration contexts where key is package
  // dependent declaration context, and value is
  // an instantiated one.
  llvm::DenseMap<DeclContext*, DeclContext*> DCsMap;

public:

  typedef DeclVisitor<PackageDeclsInstantiator, NamedDecl*> ParentTy;

  PackageDeclsInstantiator(Sema *semaObj) :
  SemaObj(semaObj), Context(semaObj->getASTContext()) {}

  // This is a package namespace instantiation method,
  // basically it should happen once per PackageDeclsInstantiator
  // life.
  NamedDecl *VisitNamespaceDecl(NamespaceDecl *PackageDependent) {
    // In case if we create instantiated AST file, we
    // should force identifier to be rewritten
    PackageDependent->getIdentifier()->setChangedSinceDeserialization();

    auto &Context = PackageDependent->getASTContext();

    NamespaceDecl *New = NamespaceDecl::Create(
        Context,
        PackageDependent->getDeclContext(),
        false,
        PackageDependent->getBeginLoc(),
        PackageDependent->getLocation(),
        PackageDependent->getIdentifier(),
        /*Previous Declaration = */PackageDependent->getMostRecentDecl()
    );

    SemaObj->addLevitationPackageDependentInstatiation(PackageDependent, New);

    auto *LexicalDC = PackageDependent->getLexicalDeclContext();

    New->setLexicalDeclContext(LexicalDC);
    LexicalDC->addDecl(New);

    mapDC(PackageDependent, New);

    return New;
  }

  NamedDecl *VisitEnumDecl(EnumDecl *PackageDependent) {
    // TODO levitation: is not implemented.
    return nullptr;
  }

  NamedDecl *VisitClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl *PackageDependent) {
    DeclContext *Owner = getInstantiatedDC(PackageDependent->getDeclContext());

    TemplateDeclInstantiator Instantiator(
            *SemaObj,
            Owner,
            MultiLevelTemplateArgumentList()
    );

    auto *New = cast<ClassTemplateSpecializationDecl>(
        Instantiator
        .VisitClassTemplateSpecializationDecl(PackageDependent)
    );

    // TODO Levitation: consider moving it out into parent Visit call.
    SemaObj->addLevitationPackageDependentInstatiation(PackageDependent, New);

    assert(
        !PackageDependent->getInstantiatedFromMemberClass() &&
        "All member class declarations are not subject of explicit "
        "package instantiation calls. Such decls should be instantiated "
        "during their parent package instantiation process"
    );

    // TemplateDeclInstantiator thinks we're instantiating member declarations,
    // and due to this assumption it calls setInstantiatedFromMember.
    // We altered behaviour of TemplateDeclInstantiator, so it
    // 1. Don't call setInstantiatedFromMember for non-members.
    // 2. Doesn't call InstantiateClass for exactly this case. It should be called
    // below.
    instantiateClass(New, PackageDependent, /*AffectDeclContext=*/false);

    return New;
  }

  NamedDecl *VisitClassTemplateDecl(
          ClassTemplateDecl *PackageDependent
  ) {
    DeclContext *Owner = getInstantiatedDC(PackageDependent->getDeclContext());
    TemplateDeclInstantiator Instantiator(
            *SemaObj,
            Owner,
            MultiLevelTemplateArgumentList()
    );

    auto *New = cast<ClassTemplateDecl>(
        Instantiator.VisitClassTemplateDecl(PackageDependent)
    );

    // TODO Levitation: consider moving it out into parent Visit call.
    SemaObj->addLevitationPackageDependentInstatiation(PackageDependent, New);

    assert(!New->getDeclContext()->isDependentContext());

    // TemplateDeclInstantiator thinks we're instantiating member declarations,
    // and due to this assumption it:
    // 1. Does setInstantiatedFromMemberTemplate.
    // 2. Postpones definition instantiation itself, till
    // InstantiateClassMembers of parent. But there is no parent and no such call.
    // So,
    // 1. We altered behaviour of TemplateDeclInstantiator
    // and don't call setInstantiatedFromMemberTemplate for non-members.
    // 2. Run pattern instantiation explicitly.

    auto *PackageDependentPattern = PackageDependent->getTemplatedDecl();
    auto *NewPattern = New->getTemplatedDecl();

    instantiateClass(
        NewPattern,
        PackageDependentPattern,
        /*AffectDeclContext=*/false
    );

    return New;
  }

  NamedDecl *VisitClassTemplatePartialSpecializationDecl(
          ClassTemplatePartialSpecializationDecl *PackageDependent
  ) {
    DeclContext *Owner = getInstantiatedDC(PackageDependent->getDeclContext());

    TemplateDeclInstantiator Instantiator(
            *SemaObj,
            Owner,
            MultiLevelTemplateArgumentList()
    );

    auto *New = cast<ClassTemplatePartialSpecializationDecl>(
        Instantiator
        .VisitClassTemplatePartialSpecializationDecl(PackageDependent)
    );

    // TODO Levitation: consider moving it out into parent Visit call.
    SemaObj->addLevitationPackageDependentInstatiation(PackageDependent, New);

    assert(
        !PackageDependent->getInstantiatedFromMemberClass() &&
        "All member class declarations are not subject of explicit "
        "package instantiation calls. Such decls should be instantiated "
        "during their parent package instantiation process"
    );

    // TemplateDeclInstantiator thinks we're instantiating member declarations,
    // and due to this assumption it:
    // 1. Does setInstantiatedFromMember.
    // 2. Postpones definition instantiation itself, till
    // InstantiateClassMembers of parent. But there is no parent and no such call.
    // So,
    // 1. We altered behaviour of TemplateDeclInstantiator
    // and don't call setInstantiatedFromMember for non-members.
    // 2. Run pattern instantiation explicitly.
    instantiateClass(New, PackageDependent, /*AffectDeclContext=*/false);

    return New;
  }

  NamedDecl *VisitCXXRecordDecl(CXXRecordDecl *PackageDependent) {
    return visitCXXRecordDeclInternal(PackageDependent);
  }

  NamedDecl *visitCXXRecordDeclInternal(
      CXXRecordDecl *PackageDependent,
      bool AffectDeclContext = true,
      bool DelayTypeCreation = false,
      bool Instantiate = true
  ) {
    // Two cases possible:
    // 1. We work with regular CXXRecordDecl
    //    (AffectDeclContext = true, PrevDecl = nullptr).
    // 2. Or we work with pattern of ClassTemplateDecl, which is represented as
    //    inner CXXRecordDecl.
    //    (AffectDeclContext = false, PrevDecl = not null
    //
    // And one more. Member class or template case.

    // Process regular class case.
    auto &Context = SemaObj->getASTContext();

    // TODO Levitation:
    // So far we can't maintain redeclaration chain with current class.
    // Direct redaclaration chain is impossible, since it supports
    // the only definition for whole chain, so whenever
    // we call "startDefinition" we forget existing definition and
    // allocate new one.
    // Postponing redeclaration is also bad idea. Whenever we instantiate new
    // class (through InstantiateClass) it also instantiates its implicit
    // redeclaration, and thus creates redeclaration chain.
    // And implicit redecl considers it "New" as "First".
    // If we just set PackageDependent to be prev for New afterwards, we
    // will build ill-formed chain.
    // This is why we only can just to remove old declaration from DC,
    // and order it to be removed whenever ASTReader reads it.
    // We should introduce concept similar to ClassTemplateDecl, which whould
    // allow ot hold new instantiated decl as specificiation.
    auto *New = CXXRecordDecl::Create(
        Context,
        PackageDependent->getTagKind(),
        VisitContext.SemanticDC,
        PackageDependent->getBeginLoc(),
        PackageDependent->getLocation(),
        PackageDependent->getIdentifier(),
        getPreviousInstantiation(PackageDependent),
        DelayTypeCreation
    );

    New->setBraceRange(PackageDependent->getBraceRange());

    New->setLexicalDeclContext(PackageDependent->getLexicalDeclContext());

    // FIXME: this whole branch 99% will never happen, make sure and remove.
    if (auto *MC = PackageDependent->getInstantiatedFromMemberClass()) {
      // Member class case.
      // FIXME: I'm not sure about whether it's correct to provide getMostRecentDecl
      //        but it seems that when we instantiate ClassTemplateDecl, we instantiate
      //        its TemplatedDecl, and all member classes, and it seems that it
      //        should provide a prev decl for that pattern.
      New->setInstantiationOfMemberClass(MC->getMostRecentDecl(), TSK_ImplicitInstantiation);

      // I'm not sure in this part.
      Context.setManglingNumber(New, Context.getManglingNumber(PackageDependent));

      if (DeclaratorDecl *DD = Context.getDeclaratorForUnnamedTagDecl(PackageDependent))
        Context.addDeclaratorForUnnamedTagDecl(New, DD);

      if (TypedefNameDecl *TND = Context.getTypedefNameForUnnamedTagDecl(PackageDependent))
        Context.addTypedefNameForUnnamedTagDecl(New, TND);
    }

    if (Instantiate) {

      SemaObj->addLevitationPackageDependentInstatiation(PackageDependent, New);

      instantiateClass(New, PackageDependent, AffectDeclContext);

      // For case of member class out of line explicit instantiation.
      New->setQualifierInfo(PackageDependent->getQualifierLoc());
    }

    return New;
  }

  NamedDecl *VisitDecl(Decl *D) {
    llvm_unreachable(
        "Declaration type is not supported. Don't know "
        "how to instantiate package dependencies."
    );
    return nullptr;
  }

  NamedDecl *Visit(NamedDecl *ND) {
    if (!(VisitContext.SemanticDC = getInstantiatedDC(ND->getDeclContext())))
      return nullptr;

    NamedDecl *New = ParentTy::Visit(ND);

    if (New) {
      if (auto *MutationListener = SemaObj->getASTMutationListener())
        MutationListener->AddedLevitationPackageInstantiation(ND, New);
    }

    return New;
  }

private:

  template<typename DeclTy>
  DeclTy *getPreviousInstantiation(DeclTy *PackageDependent) {
    auto *ExternalSource = SemaObj->getExternalSource();

    if (!ExternalSource)
      return nullptr;

    SmallVector<NamedDecl *, 1> Instantiations;
    ExternalSource->ReadLevitationPackageInstantiations(PackageDependent, Instantiations);

    if (Instantiations.empty())
      return nullptr;

    // TODO Levitation: emit diag error.
    assert(Instantiations.size() == 1 && "Only one previous instantiation is allowed");

    return cast<DeclTy>(Instantiations.front());
  }

  void mapDC(DeclContext *Dependent, DeclContext *New) {
    DCsMap.insert({Dependent, New});
  }

  DeclContext* getInstantiatedDC(DeclContext *OriginalDC) {
    if (!OriginalDC->isPackageDependentContext())
      return OriginalDC;

    auto Found = DCsMap.find(OriginalDC);
    if (Found == DCsMap.end())
      return nullptr;

    return Found->second;
  }

  void instantiateClass(
      CXXRecordDecl *New,
      CXXRecordDecl *PackageDependent,
      bool AffectDeclContext = true
  ) {

    if (AffectDeclContext) {
      // If declaration has been loaded from AST,
      // force DeclContext lazy decls collection to load it.

      auto *LexicalDC = PackageDependent->getLexicalDeclContext();
      LexicalDC = addToInstantiatedDeclContext(LexicalDC, New);

      assert(
          LexicalDC &&
          "Declaration contexts should be instantiated before their children"
      );
    }

    auto PointOfInstantiation = PackageDependent->getBeginLoc();

    MultiLevelTemplateArgumentList EmptyArgsList;
    SemaObj->InstantiateClass(
        PointOfInstantiation,
        New,
        PackageDependent,
        EmptyArgsList,
        TSK_ExplicitInstantiationDefinition
    );

    // Instantiate package references of PackageDependent
    // into New.

    auto *Def = New->getDefinition();

    SemaObj->InstantiateClassMembers(
        PointOfInstantiation,
        Def,
        EmptyArgsList,
        TSK_ExplicitInstantiationDefinition
    );

    mapDC(PackageDependent, New);
  }

  DeclContext *addToInstantiatedDeclContext(DeclContext *DC, NamedDecl *D) {

    auto *MappedDC = getInstantiatedDC(DC);
    if (!MappedDC)
      return nullptr;

    D->setLexicalDeclContext(MappedDC);
    MappedDC->addDecl(D);
    return MappedDC;
  }

  void setAdditionalSpecializationProperties(
      ClassTemplateSpecializationDecl *NewSpecialization,
      ClassTemplateSpecializationDecl *Specialization
  ) {
    NewSpecialization->setSpecializationKind(TSK_ExplicitSpecialization);

    // I think we should clone it during instantiation stage
    // ProcessDeclAttributeList(...);

    // if (TUK == TUK_Definition && (!SkipBody || !SkipBody->ShouldSkip)) {
    //   AddAlignmentAttributesForRecord(Specialization);
    //   AddMsStructLayoutForRecord(Specialization);
    // }

    // Build the fully-sugared type for this class template
    // specialization as the user wrote in the specialization
    // itself. This means that we'll pretty-print the type retrieved
    // from the specialization's declaration the way that the user
    // actually wrote the specialization, rather than formatting the
    // name based on the "canonical" representation used to store the
    // template arguments in the specialization.
    NewSpecialization->setTypeAsWritten(Specialization->getTypeAsWritten());
    NewSpecialization->setTemplateKeywordLoc(Specialization->getTemplateKeywordLoc());

    // C++ [temp.expl.spec]p9:
    //   A template explicit specialization is in the scope of the
    //   namespace in which the template was defined.
    //
    // We actually implement this paragraph where we set the semantic
    // context (in the creation of the ClassTemplateSpecializationDecl),
    // but we also maintain the lexical context where the actual
    // definition occurs.
    NewSpecialization->setLexicalDeclContext(Specialization->getLexicalDeclContext());

    // For explicit instantiation
    NewSpecialization->setBraceRange(Specialization->getBraceRange());

    // May be it is a member class specialization?
    if (Specialization->getInstantiatedFromMemberClass()) {
      NewSpecialization->setInstantiationOfMemberClass(Specialization, TSK_ImplicitInstantiation);
    }

    // We use redeclaration mechanics in order to make it available
    // in existing ClassTemplateDecl owner
    // (it will replace package dependent decl).
    NewSpecialization->setPreviousDecl(Specialization);
  }
};

void Sema::InstantiatePackageClasses() {

  // As long as we going to remove some old decls after dependencies instantiation
  // we can't do it in single delcs visiting, for removal affects decl
  // iterator.
  //
  // So, we should do it in two passes:
  // 1. Add package dependent declarations into separate collection.
  // 2. Go through collection, and instantiate its items.

  if (!ExternalSource)
    return;

  SmallVector<NamedDecl*, 8> LevitationPackageDependentDecls;

  PackageDependentClassesMarker Search([&] (NamedDecl *ND) {
    LevitationPackageDependentDecls.push_back(ND);
  });
  Search.TraverseDecl(Context.getTranslationUnitDecl());

  if (LevitationPackageDependentDecls.empty())
    return;

  assert(
      Search.getPackageNamespaces().size() == 1 &&
      "Only one package namespace allowed per instantiation stage"
  );

  NamespaceDecl *PackageNamespace = *Search.getPackageNamespaces().begin();

  SmallVector<NamedDecl*, 8> ToBeInstantiated;
  SmallVector<NamedDecl*, 8> OutOfScopeMemberDecls;

  for (auto *D : LevitationPackageDependentDecls) {
    // If it is an out of scope member class definition, skip it,
    // it should be instantiated as a member, during
    // InstaniateClassMembers stage.
    if (D->getDeclContext()->isRecord() &&
        !D->hasAttr<ExcludeFromExplicitInstantiationAttr>()) {
      OutOfScopeMemberDecls.push_back(D);
    }

    ToBeInstantiated.push_back(D);
  }

  PackageDeclsInstantiator Instantiator(this);
  Instantiator.Visit(PackageNamespace);

  {
    // Create a local instantiation scope for this class template, which
    // will contain the instantiations of the template parameters.
    LocalInstantiationScope Scope(*this);

    if (ToBeInstantiated.size()) {
      for (auto *D : ToBeInstantiated) {
        Instantiator.Visit(D);
      }
    }
  }

  PerformPendingInstantiations();
}
