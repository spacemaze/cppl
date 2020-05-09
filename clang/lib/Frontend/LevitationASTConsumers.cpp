//==-- LevitationASTConsumers.cpp -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//
// Levitation AST Consumers Implementations.
//
//===----------------------------------------------------------------------===//

#include "clang/Frontend/LevitationASTConsumers.h"
#include "clang/AST/AST.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticFrontend.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Levitation/Common/File.h"
#include "clang/Levitation/Common/Path.h"
#include "clang/Levitation/Common/Utility.h"
#include "clang/Levitation/DeclASTMeta/DeclASTMeta.h"
#include "clang/Levitation/Dependencies.h"
#include "clang/Levitation/FileExtensions.h"
#include "clang/Levitation/Serialization.h"
#include "clang/Lex/Preprocessor.h"

#include "llvm/Bitstream/BitstreamWriter.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"

#include <utility>

namespace clang {
namespace levitation {

  void LevitationMultiplexPreprocessorConsumer::HandlePreprocessor(
      clang::Preprocessor &PP
  ) {
    for (auto &ConsumerPP : Consumers)
      ConsumerPP->HandlePreprocessor(PP);
  }

  class LevitationInputFileProcessor {
  protected:
    const CompilerInstance &CI;
    SinglePath CurrentInputFileRel;

    LevitationInputFileProcessor(
        const CompilerInstance &ci,
        SinglePath&& currentInputFileRel
    )
    : CI(ci),
      CurrentInputFileRel(std::move(currentInputFileRel))
    {
      if (llvm::sys::path::is_absolute(CurrentInputFileRel)) {
        llvm::errs()
        << "Invalid path:\n"
        << "    " << CurrentInputFileRel.str() << "\n";
        llvm_unreachable("Input file path should be relative (to source dir parameter)");
      }
    }
  };

  class ASTDependenciesProcessor :
      public LevitationInputFileProcessor,
      public LevitationPreprocessorConsumer {
  public:
    ASTDependenciesProcessor(
        const CompilerInstance &ci, SinglePath&& currentInputFileRel
    )
    : LevitationInputFileProcessor(ci, std::move(currentInputFileRel)) {}

    void HandlePreprocessor(Preprocessor &PP) override {

      PackageDependencies& Dependencies = PP.accessLevitationDependencies();
      Dependencies.setPackageFilePathID(CurrentInputFileRel);

      auto F = createFile();

      if (auto OpenedFile = F.open()) {
        buffer_ostream buffer(OpenedFile.getOutputStream());
        auto Writer = CreateBitstreamWriter(buffer);
        Writer->writeAndFinalize(Dependencies);

        writeMeta(
            CI.getFrontendOpts().LevitationDeclASTMeta,
            buffer.str()
        );
      }

      if (F.hasErrors()) {
        diagDependencyFileIOIssues(F.getStatus());
      }
    }
  private:
    File createFile() {
      return File(CI.getFrontendOpts().LevitationDependenciesOutputFile);
    }

    void writeMeta(StringRef MetaOut, StringRef LDepsBuffer) {
      // Remember everything we need for pos-processing.
      // After FrontendAction::EndSourceFile()
      // Compiler invocation and Sema will be destroyed.
      // Only SourceManager and FileManager remain.

      auto &SM = CI.getSourceManager();
      auto SrcBuffer = SM.getBufferData(SM.getMainFileID());

      auto SourceMD5 = levitation::calcMD5(SrcBuffer);
      auto OutputMD5 = levitation::calcMD5(LDepsBuffer);

      levitation::DeclASTMeta Meta(
        SourceMD5.Bytes,
        OutputMD5.Bytes,
        DeclASTMeta::FragmentsVectorTy()
      );

      assert(MetaOut.size());
      levitation::File F(MetaOut);

      if (auto OpenedFile = F.open()) {
        auto Writer = levitation::CreateMetaBitstreamWriter(OpenedFile.getOutputStream());
        Writer->writeAndFinalize(Meta);
      }

      if (F.hasErrors()) {
        CI.getDiagnostics().Report(
            diag::err_fe_levitation_dependency_file_failed_to_create_meta
        );
        return;
      }
    }

    void diagDependencyFileIOIssues(File::StatusEnum status) {
      auto &Diag = CI.getDiagnostics();
      switch (status) {
        case File::HasStreamErrors:
          Diag.Report(diag::err_fe_levitation_dependency_file_io_troubles);
          break;
        case File::FiledToRename:
        case File::FailedToCreateTempFile:
          Diag.Report(diag::err_fe_levitation_dependency_file_failed_to_create);
          break;
        default:
          break;
      }
    }
  };

std::unique_ptr<LevitationPreprocessorConsumer> CreateDependenciesASTProcessor(
    CompilerInstance &CI,
    StringRef InFile
) {
  if (CI.getFrontendOpts().LevitationDependenciesOutputFile.empty())
    return nullptr;

  // TODO Levitation: remove this field.
  //   We only need this for PackagePathID, we we don't need latter anymore.
  //   Whenever we pick LDeps file, we also know PackageID it corresponds to.
  auto InFileRel = levitation::Path::makeRelative<SinglePath>(
      InFile,
      CI.getFrontendOpts().LevitationSourcesRootDir
  );

  return std::make_unique<ASTDependenciesProcessor>(CI, std::move(InFileRel));
}

}}
