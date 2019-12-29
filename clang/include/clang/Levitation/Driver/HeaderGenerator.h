//===--- HeaderGenerator.h - C++ HeaderGenerator class ----------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file contains .h files generator class
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_HEADERGENERATOR_H
#define LLVM_LEVITATION_HEADERGENERATOR_H

#include "llvm/ADT/StringRef.h"
#include "clang/Basic/FileManager.h"
#include "clang/Levitation/Common/CreatableSingleton.h"
#include "clang/Levitation/Common/File.h"
#include "clang/Levitation/FileExtensions.h"
#include "clang/Levitation/Common/Path.h"
#include "clang/Levitation/Common/SimpleLogger.h"
#include "clang/Levitation/Common/Utility.h"
#include "clang/Levitation/DeclASTMeta/DeclASTMeta.h"
#include "clang/Levitation/Driver/Dump.h"

namespace clang { namespace levitation { namespace tools {

class HeaderGenerator {
  llvm::StringRef OutputFile;
  llvm::StringRef SourceFile;
  llvm::StringRef Preamble;
  Twine SourceFileFullPath;
  Twine OutputFileFullPath;
  const Paths& Includes;
  const DeclASTMeta::FragmentsVectorTy& SkippedBytes;
  bool Verbose;
  bool DryRun;
  log::Logger &Log;

public:
  HeaderGenerator(
      llvm::StringRef OutputFile,
      const llvm::StringRef &SourceFile,
      StringRef Preamble,
      const Paths &Includes,
      const DeclASTMeta::FragmentsVectorTy &SkippedBytes,
      bool Verbose,
      bool DryRun
  )
  : OutputFile(OutputFile),
    SourceFile(SourceFile),
    Preamble(Preamble),
    Includes(Includes),
    SkippedBytes(SkippedBytes),
    Verbose(Verbose),
    DryRun(DryRun),
    Log(log::Logger::get())
  {}

  std::unique_ptr<MemoryBuffer> getSourceFileBuffer() {
    // return InputFile("$SourcesRoot/$SourceFile")
    auto &FM = CreatableSingleton<FileManager>::get();
    auto MemBufOrErr = FM.getBufferForFile(SourceFile);

    if (!MemBufOrErr)
      return nullptr;

    return std::move(MemBufOrErr.get());
  }

  bool execute() {
    if (Verbose)
      dump(Log.verbose());
    else if (DryRun)
      dump(Log.info());

    if (DryRun)
      return true;

    if (auto InPtr = getSourceFileBuffer()) {
      const auto &In = *InPtr;
      const char *InStart = In.getBufferStart();
      size_t InSize = In.getBufferSize();

      File OutF(OutputFile);
      if (auto OpenedFile = OutF.open()) {
        auto &out = OpenedFile.getOutputStream();

        emitHeadComment(out);

        emitIncludes(out);

        emitAfterIncludesComment(out);

        size_t Start = 0;
        StringRef NewLine("\n");
        for (const auto &skippedRange : SkippedBytes) {
          // possible skip cases:
          //
          // Case 1:
          // [keep part] [skip part] [keep part]
          //
          // Case 2:
          // [keep] [skip]
          // [keep (new line)]
          //
          // Case 3:
          // [keep]
          // [skip] [keep]
          //
          // Case 4:
          // [keep]
          // [skip]
          // [keep]


          assert(InSize - Start >= skippedRange.size());

          // Detect new line in the end of [keep] fragment.
          // If present, strip trailing spaces, but remember indentation.
          auto *KeepPtr = InStart + Start;
          size_t KeepWriteCount = skippedRange.Start - Start;
          size_t KeepWriteCountStripped = KeepWriteCount;

          bool AfterKeepNewLine = false;

          stripTrailingSpaces(KeepPtr, KeepWriteCountStripped);

          size_t AfterKeepSpaces = KeepWriteCount - KeepWriteCountStripped;

          StringRef Keep(KeepPtr, KeepWriteCountStripped);

          if (Keep.endswith(NewLine)) {
            KeepWriteCountStripped -= NewLine.size();
            AfterKeepNewLine = true;
          }

          KeepWriteCount = KeepWriteCountStripped;

          out.write(KeepPtr, KeepWriteCount);

          if (skippedRange.ReplaceWithSemicolon)
            out << ";";

          Start = skippedRange.End;

          // If skipped fragment was ended with new line, or \n\s+
          // preserve new line, but not trailing spaces.
          auto SkippedFragmentStartPtr = InStart + skippedRange.Start;
          auto SkippedFragmentSize = skippedRange.End - skippedRange.Start;
          auto OldSize = SkippedFragmentSize;
          bool AfterSkipNewLine = false;

          stripTrailingSpaces(SkippedFragmentStartPtr, SkippedFragmentSize);

          StringRef Skip(
              SkippedFragmentStartPtr, SkippedFragmentSize
          );

          auto AfterSkipSpaces = OldSize - SkippedFragmentSize;

          if (Skip.endswith(NewLine))
            AfterSkipNewLine = true;

          // Case 1: [keep] and [skip] on same line:
          //   emit spaces we found after [skip]
          if (!AfterKeepNewLine && !AfterSkipNewLine) {
            out.indent((unsigned) AfterSkipSpaces);
          } else

          // Case 2: [keep] [skip] \n [some spaces]
          // Case 4: [keep]\n  [skip]\n
          // Do the same for both cases:
          //   emit new line
          //   emit spaces after skip
          if (AfterSkipNewLine) {
            out << "\n";
            out.indent((unsigned)AfterSkipSpaces);
          } else

          // Case 3: [keep]\n  [skip (without new line)]
          //   emit new line
          //   emit spaces after keep
          {
            out << "\n";
            out.indent((unsigned)AfterKeepSpaces);
          }

          if (skippedRange.PrefixWithExtern)
            out << "extern ";
        }

        auto KeepPtr = InStart + Start;
        auto KeepWriteCount = InSize - Start;

        stripTrailingSpaces(KeepPtr, KeepWriteCount);

        out.write(KeepPtr, KeepWriteCount);
      }

      if (OutF.hasErrors()) {
        diagOutFileIOIssues(OutF.getStatus());
        return false;
      }
    } else {
      diagInFileIOIssues();
      return false;
    }

    return true;
  }

protected:

  void diagInFileIOIssues() {
    Log.error() << "Failed to open file '" << SourceFileFullPath << "'\n";
  }

  void diagOutFileIOIssues(File::StatusEnum Status) {
      auto &err = Log.error()
      << "Failed to open file '" << OutputFileFullPath << "': ";

      switch (Status) {
        case File::HasStreamErrors:
          err << "stream error.";
          break;
        case File::FiledToRename:
          err << "temp file created, but failed to rename.";
          break;
        case File::FailedToCreateTempFile:
          err << "failed to create temp file.";
          break;
        default:
          err << "unknown reason.";
          break;
      }
      err << "\n";
  }

  void emitHeadComment(llvm::raw_ostream &out) {
    out << "//===--------------------- C++ Levitation generated file --------*- C++ -*-===//\n"
        << "//\n"
        << "//                             Don't edit this file.\n"
        << "//\n"
        << "//===----------------------------------------------------------------------===//\n\n";
  }

  void emitAfterIncludesComment(llvm::raw_ostream &out) {
    out << "// C++ Levitation: below follows stripped ."
        << FileExtensions::SourceCode
        << " file contents.\n\n";
  }

  void emitIncludes(llvm::raw_ostream &out) {

    if (Includes.empty() && Preamble.empty())
      return;

    if (!Preamble.empty()) {
      out << "// C++ Levitation: preamble\n";
      out << "#include \"" << Preamble << "\"\n\n";
    }

    if (Includes.empty())
      return;

    out << "// C++ Levitation: below are #include directives for all dependencies\n\n";

    for (const auto &inc : Includes) {
      out << "#include \"" << inc << "\"\n";
    }

    out << "\n";
  }

  bool stripTrailingSpaces(const char *Str, size_t &Size) {
    size_t OldSize = Size;
    while (Size && Str[Size-1] == ' ') --Size;
    return Size != OldSize;
  }

  void dump(llvm::raw_ostream &out) {
    DriverPhaseDump::action(
        out, OutputFile, SourceFile, Includes, "GEN HEADER", ".h"
    );
  }
};

}}}

#endif //LLVM_LEVITATION_HEADERGENERATOR_H
