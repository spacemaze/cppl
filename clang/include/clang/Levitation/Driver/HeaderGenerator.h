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
      const Paths &Includes,
      const DeclASTMeta::FragmentsVectorTy &SkippedBytes,
      bool Verbose,
      bool DryRun
  )
  : OutputFile(OutputFile),
    SourceFile(SourceFile),
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
        bool WriteNewLine;
        for (const auto &skippedRange : SkippedBytes) {

          assert(InSize - Start >= skippedRange.size());

          writeFragment(
              out,
              InStart + Start,
              skippedRange.Start - Start,
              WriteNewLine
          );

          // Now write skipped fragment remnants.

          // If it was requested to replace skipped fragment with
          // ';' do it.
          if (skippedRange.ReplaceWithSemicolon)
            out << ";";

          Start = skippedRange.End;

          // If skipped fragment was ended with new line, or \n\s+
          // preserve new line, but not trailing spaces.
          auto SkippedFragmentStartPtr = InStart + skippedRange.Start;
          auto SkippedFragmentSize = skippedRange.End - skippedRange.Start;

          stripTrailingSpaces(SkippedFragmentStartPtr, SkippedFragmentSize);

          size_t OldSize = SkippedFragmentSize;
          StringRef SkippedFragmentStr(
              SkippedFragmentStartPtr, SkippedFragmentSize
          );

          // We need to emit '\n' if it was stripped from
          // fragment-to-write or if it was in skipped fragment's tail
          if (SkippedFragmentStr.endswith("\n") || WriteNewLine) {
            out.indent(OldSize - SkippedFragmentSize);
            out << "\n";
          }
        }

        writeFragment(
            out,
            InStart + Start,
            InSize - Start,
            WriteNewLine
        );

        if (WriteNewLine)
          out << "\n";
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

    if (Includes.empty())
      return;

    out << "// C++ Levitation: below are #include directives for all dependencies\n\n";

    for (const auto &inc : Includes) {
      out << "#include \"" << inc << "\"\n";
    }

    out << "\n";
  }

  void writeFragment(
      llvm::raw_ostream& out,
      const char *WritePtr,
      size_t WriteCount,
      bool &WriteNewLine
  ) {

    // Fragment we're about to write, may end with trailing spaces:
    // like this: \s+\n
    // In this case, strip that suffix, but remember we stripped new line
    // as well.
    WriteNewLine = correctIfTrailingSpaces(WritePtr, WriteCount);
    out.write(WritePtr, WriteCount);
  }

  bool stripTrailingSpaces(const char *Str, size_t &Size) {
    size_t OldSize = Size;
    while (Size && Str[Size-1] == ' ') --Size;
    return Size != OldSize;
  }

  bool correctIfTrailingSpaces(const char *SourceStart, size_t &End) {
    StringRef NewLineStr = StringRef("\n");
    size_t NewLineLen = NewLineStr.size();

    // If string to short to be ended with " \n", boil out.
    if (End < NewLineLen)
      return false;

    // If string is not ended with new line, boil out.
    if (StringRef(SourceStart + End - NewLineLen, NewLineLen) != NewLineStr)
      return false;

    size_t NewEnd = End - NewLineLen;

    if (!stripTrailingSpaces(SourceStart, NewEnd))
      return false;

    End = NewEnd;
    return true;
  }

  void dump(llvm::raw_ostream &out) {
    DriverPhaseDump::action(
        out, OutputFile, SourceFile, Includes, "GEN HEADER", ".h"
    );
  }
};

}}}

#endif //LLVM_LEVITATION_HEADERGENERATOR_H
