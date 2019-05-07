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
#include "clang/AST/NestedNameSpecifier.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Sema/SemaInternal.h"
#include "clang/Sema/Template.h"
#include "llvm/Support/Path.h"

#include <iterator>
using namespace clang;
using namespace sema;

//===--------------------------------------------------------------------===//
// Dependency handling
//
// Group of methods below implements
// package dependencies detection and handling.
//

struct PackageDependencyInfo {
    const Decl* DependentDecl;
    const NestedNameSpecifierLoc& Loc;
    const IdentifierInfo *Name;
    const StringRef FullFileName;
};

/// Adds dependency for declaration DependentDecl.
/// E.g. for 'class C { global::A::D d; };' it will add
/// dependency A::D, (Loc will be 'global::A', Name will be 'D'.
///
/// \param DependentDecl declaration dependency is added for
/// \param Loc nested name specifier of dependency
/// \param Name dependency identifier.
void addLevitationPackageDeclarationDependency(const PackageDependencyInfo &Dep) {
}

/// Adds body-dependency for declaration DependentDecl.
/// E.g. for 'class C { void Foo() { global::A::D d; } };' it will add
/// dependency A::D, (Loc will be 'global::A', Name will be 'D'.
/// Note: "body-dependency" means that only method bodies of DependentDecl
/// uses it.
///
/// \param DependentDecl declaration dependency is added for
/// \param Loc nested name specifier of dependency
/// \param Name dependency identifier.
void addLevitationPackageBodyDependency(const PackageDependencyInfo &Dep) {
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

StringRef getMostRightComponentName(NestedNameSpecifier *NNS) {

  switch (NNS->getKind()) {
  case NestedNameSpecifier::Identifier:
    return NNS->getAsIdentifier()->getName();

  case NestedNameSpecifier::Namespace:
    if (NNS->getAsNamespace()->isAnonymousNamespace())
      llvm_unreachable("Anonymous namespace? As a dependency prefix?");
    return NNS->getAsNamespace()->getName();

  case NestedNameSpecifier::NamespaceAlias:
    return NNS->getAsNamespaceAlias()->getName();

  case NestedNameSpecifier::Global:
  llvm_unreachable("Global NNS can't be a dependency prefix."
                   " 'global' keyword should be used instead.");
  break;

  case NestedNameSpecifier::Super:
    llvm_unreachable("__super can't be a dependency prefix");
    break;

  case NestedNameSpecifier::TypeSpecWithTemplate:
    // Fall through to print the type.
    LLVM_FALLTHROUGH;

  case NestedNameSpecifier::TypeSpec: {
    const auto *Record =
            dyn_cast_or_null<ClassTemplateSpecializationDecl>(NNS->getAsRecordDecl());

    if (Record)
        // Print the type trait with resolved template parameters.
        return Record->getName();

    // TODO levitation

    llvm_unreachable("TODO");
    break;
//    const Type *T = getAsType();
//
//    // Nested-name-specifiers are intended to contain minimally-qualified
//    // types. An actual ElaboratedType will not occur, since we'll store
//    // just the type that is referred to in the nested-name-specifier (e.g.,
//    // a TypedefType, TagType, etc.). However, when we are dealing with
//    // dependent template-id types (e.g., Outer<T>::template Inner<U>),
//    // the type requires its own nested-name-specifier for uniqueness, so we
//    // suppress that nested-name-specifier during printing.
//    assert(!isa<ElaboratedType>(T) &&
//           "Elaborated type in nested-name-specifier");
//    if (const TemplateSpecializationType *SpecType
//          = dyn_cast<TemplateSpecializationType>(T)) {
//      // Print the template name without its corresponding
//      // nested-name-specifier.
//      SpecType->getTemplateName().print(OS, InnerPolicy, true);
//
//      // Print the template argument list.
//      printTemplateArgumentList(OS, SpecType->template_arguments(),
//                                InnerPolicy);
//    } else {
//      // Print the type normally
//      QualType(T, 0).print(OS, InnerPolicy);
//    }
//    break;
  }
  }
}

bool isDirectory(const SmallVectorImpl<char> &Dir) {
  // TODO levitation
  llvm_unreachable("TODO");
  return true;
}

void diagMissingDependency(
    const NestedNameSpecifierLoc &Loc,
    const IdentifierInfo *Name) {
  // TODO levitation
  llvm_unreachable("TODO");
}

bool isFile(
    const SmallVectorImpl<char> &BaseDir,
    StringRef FileName
) {
  // TODO levitation
  llvm_unreachable("TODO");
  return false;
}

std::string getFullFileName(
    const SmallVectorImpl<char> &BaseDir,
    StringRef FileName
) {
  if (!isFile(BaseDir, FileName))
    return std::string();
  // TODO levitation
  llvm_unreachable("TODO");
  return std::string();
}

// FIXME: We should move this to stage where we have an access
//        to FileManager.
std::string computePath(
    const NestedNameSpecifierLoc &Loc,
    const IdentifierInfo *Name) {

  SmallVector<StringRef, 8> NNSComponentsRev;

  for (NestedNameSpecifier *Cur = Loc.getNestedNameSpecifier();
       Cur; Cur = Cur->getPrefix()) {
    NNSComponentsRev.push_back(getMostRightComponentName(Cur));
  }

  SmallVector<char, 256> Base;
  SmallVector<char, 256> NewBase;
  StringRef FileName;

  for (auto ComponentIt = NNSComponentsRev.rbegin(), e = NNSComponentsRev.rend();
       ComponentIt != e; ++ComponentIt) {

    auto Component = *ComponentIt;

    auto AppendStart = NewBase.size();

    llvm::sys::path::append(NewBase, Component);

    auto AppendEnd = NewBase.size();

    if (isDirectory(NewBase)) {
      Base.append(&NewBase[AppendStart], &NewBase[AppendEnd]);
    } else {
      FileName = Component;
      break;
    }
  }

  if (Base.empty() || FileName.empty())
    diagMissingDependency(Loc, Name);

  auto FullFileName = getFullFileName(Base, FileName);
  if (FullFileName.empty())
    diagMissingDependency(Loc, Name);

  return FullFileName;
}

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

  std::string FilePath = computePath(Loc, Name);
  if (FilePath.empty())
    return false;

// TODO Levitation
//  if (IsBodyDependency)
//    addLevitationPackageBodyDependency({TD, Loc, Name, FilePath});
//  else
//    addLevitationPackageDeclarationDependency({TD, Loc, Name, FilePath});

  auto &out = llvm::errs();
  TD->printQualifiedName(out);
  out << " ";
  TD->getLocation().print(out, SourceMgr);
  out << " depends on ";
  Loc.getNestedNameSpecifier()->dump(out);
  out << ":";
  out << Name->getName() << ", ";
  Loc.getLocalBeginLoc().print(out, SourceMgr);

  if (IsBodyDependency) {
    out << " [definition only, for ";
    OutermostF->printQualifiedName(out);
    out << "]";
  }

  if (IsBuildingSpecification)
    out << " [through its template specification] ";

  out << "\n";

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
        // We do nothing.
        // Basically we just keep LevitationPackageDependent = false
      }
      void VisitClassTemplateDecl(ClassTemplateDecl *D) {
        // We do nothing.
        // Basically we just keep LevitationPackageDependent = false
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

public:

    PackageDependentClassesMarker(MarkActionTy markAction) : MarkAction(markAction) {}

    bool TraverseNamespaceDecl(NamespaceDecl *NS) {
      if (!NS->isLevitationPackage())
        return ParentTy::TraverseNamespaceDecl(NS);

      PackageDependentDeclsVisitor DependentDeclsMarker(MarkAction);
      for (auto *D : NS->decls())
         DependentDeclsMarker.Visit(D);

    return true;
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

Decl *Sema::substLevitationPackageDependentDecl(const Decl *D) {
  if (getLangOpts().getLevitationBuildStage() == LangOptions::LBSK_BuildObjectFile &&
      D->isLevitationPackageDependent()) {
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

class PackageDeclsInstantiator : public DeclVisitor<PackageDeclsInstantiator, Decl*> {
private:
  Sema *SemaObj;
  ASTContext &Context;

public:
  PackageDeclsInstantiator(Sema *semaObj) :
  SemaObj(semaObj), Context(semaObj->getASTContext()) {}

  Decl *VisitEnumDecl(EnumDecl *PackageDependent) {
    // TODO levitation: is not implemented.
    return nullptr;
  }

  Decl *VisitClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl *PackageDependent) {

    auto *SemanticDC = PackageDependent->getDeclContext();

    auto &TemplateArgs = PackageDependent->getTemplateArgs();

    auto *Template = PackageDependent->getSpecializedTemplate();
    auto *New = ClassTemplateSpecializationDecl::Create(
        Context,
        PackageDependent->getTagKind(),
        SemanticDC,
        PackageDependent->getBeginLoc(),
        PackageDependent->getLocation(),
        Template,
        TemplateArgs.asArray(),
        // Postpone setting previous declaration.
        // If we set it now, then it will use the same DefinitinoData
        // and we need it to be different.
        nullptr
    );

    if (Template->getTemplateParameters()->size() > 0) {
      PackageDependent->setTemplateParameterListsInfo(
          Context,
          Template->getTemplateParameters()
      );
    }

    setAdditionalSpecializationProperties(New, PackageDependent);

    instantiateClass(New, PackageDependent);

    return New;
  }

  // FIXME levitation: I think we don't need this, but keep for now, for better
  // commit diffs.
  Decl *VisitClassTemplatePartialSpecializationDecl(
          ClassTemplatePartialSpecializationDecl *PackageDependent
  ) {
    auto &Context = SemaObj->getASTContext();
    auto &TemplateArgs = PackageDependent->getTemplateArgs();
    ClassTemplateDecl* Template = PackageDependent->getDescribedClassTemplate();

    TemplateArgumentListInfo ArgsAsWritten;
    for (auto A : PackageDependent->getTemplateArgsAsWritten()->arguments()) {
      ArgsAsWritten.addArgument(A);
    }

    ClassTemplatePartialSpecializationDecl *New = ClassTemplatePartialSpecializationDecl::Create(
        Context,
        PackageDependent->getTagKind(),
        PackageDependent->getDeclContext(),
        PackageDependent->getBeginLoc(),
        PackageDependent->getLocation(),

        // FIXME: we may probably bump into scope issue.
        // But probably it will keep scope of previous decl.
        PackageDependent->getTemplateParameters(),
        Template,
        TemplateArgs.asArray(),
        ArgsAsWritten,
        QualType(PackageDependent->getTypeForDecl(), 0),
        // Postpone setting previous declaration.
        // If we set it now, then it will use the same DefinitinoData
        // and we need it to be different.
        nullptr
    );

    setAdditionalSpecializationProperties(New, PackageDependent);

    instantiateClass(New, PackageDependent);

    return New;
  }

  Decl *VisitCXXRecordDecl(CXXRecordDecl *PackageDependent) {
    visitCXXRecordDeclInternal(PackageDependent);
  }

  Decl *visitCXXRecordDeclInternal(
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

    auto *New = CXXRecordDecl::Create(
        Context,
        PackageDependent->getTagKind(),
        PackageDependent->getDeclContext(),
        PackageDependent->getBeginLoc(),
        PackageDependent->getLocation(),
        PackageDependent->getIdentifier(),
        // Postpone setting previous declaration.
        // If we set it now, then it will use the same DefinitinoData
        // and we need it to be different.
        nullptr,
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

  Decl *VisitDecl(Decl *D) {
    llvm_unreachable(
        "Declaration type is not supported. Don't know "
        "how to instantiate package dependencies."
    );
    return nullptr;
  }

private:

  void instantiateClass(
      CXXRecordDecl *New,
      CXXRecordDecl *PackageDependent,
      bool AffectDeclContext = true
  ) {

    auto *LexicalDC = PackageDependent->getLexicalDeclContext();

    if (AffectDeclContext) {
      // If declaration has been loaded from AST,
      // force DeclContext lazy decls collection to load it.
      LexicalDC->containsDeclAndLoad(PackageDependent);
      LexicalDC->removeDecl(PackageDependent);
      LexicalDC->addDecl(New);
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
  ExternalSource->ReadLevitationPackageDependentDecls(LevitationPackageDependentDecls);

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

    if (auto *DC = dyn_cast<DeclContext>(D)) {
      assert(!DC->getLookupPtr() &&
      "Package instantiation should be launched before any lookups");
    }

    // During package instantiation we should consider decls as
    // package dependent.
    D->setLevitationPackageDependent(true);

    ToBeInstantiated.push_back(D);
  }

  PackageDeclsInstantiator Instantiator(this);

  for (auto *D : ToBeInstantiated) {
    Instantiator.Visit(D);
  }

  for (auto *D : OutOfScopeMemberDecls) {
    D->getLexicalDeclContext()->removeDecl(D);
  }
}
