//===--- C++ Levitation Driver.cpp ------------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file contains implementation for C++ Levitation Driver methods
//
//===----------------------------------------------------------------------===//

#include "clang/Basic/FileManager.h"

#include "clang/Levitation/Common/Failable.h"
#include "clang/Levitation/Common/File.h"
#include "clang/Levitation/Common/FileSystem.h"
#include "clang/Levitation/Common/SimpleLogger.h"
#include "clang/Levitation/Common/StringBuilder.h"
#include "clang/Levitation/DeclASTMeta/DeclASTMeta.h"
#include "clang/Levitation/DeclASTMeta/DeclASTMetaLoader.h"
#include "clang/Levitation/DependenciesSolver/DependenciesGraph.h"
#include "clang/Levitation/DependenciesSolver/DependenciesSolverPath.h"
#include "clang/Levitation/DependenciesSolver/DependenciesSolver.h"
#include "clang/Levitation/DependenciesSolver/SolvedDependenciesInfo.h"
#include "clang/Levitation/Driver/Driver.h"
#include "clang/Levitation/Driver/HeaderGenerator.h"
#include "clang/Levitation/FileExtensions.h"
#include "clang/Levitation/TasksManager/TasksManager.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Program.h"
#include "llvm/ADT/None.h"

#include <algorithm>
#include <memory>
#include <system_error>
#include <utility>

namespace clang { namespace levitation { namespace tools {

using namespace clang::levitation::dependencies_solver;
using namespace clang::levitation::tasks;

//-----------------------------------------------------------------------------
//  Levitation driver implementation classes
//  (LevitationDriver itself is defined at the bottom)

namespace {

  // TODO Levitation: Whole Context approach is malformed.
  // Context should keep shared data for all sequence steps.
  // If something is required for particular step only it should
  // be out of context.

  struct FilesInfo {
    SinglePath Source;
    SinglePath Header;
    SinglePath LDeps;
    SinglePath LDepsMeta;
    SinglePath DeclASTMetaFile;
    SinglePath ObjMetaFile;

    // FIXME Levitation: deprecated
    SinglePath AST;

    SinglePath DeclAST;
    SinglePath Object;
  };

  struct RunContext {

    // TODO Levitation: Introduce LevitationDriverOpts and use here
    // its reference instead.
    LevitationDriver &Driver;
    Failable Status;

    Paths Packages;

    // TODO Levitation: key may be an integer (string ID for PackagePath)
    llvm::DenseMap<StringRef, FilesInfo> Files;

    std::shared_ptr<SolvedDependenciesInfo> DependenciesInfo;

    bool PreambleUpdated = false;
    bool ObjectsUpdated = false;
    DependenciesGraph::NodesSet UpdatedNodes;

    RunContext(LevitationDriver &driver)
    : Driver(driver)
    {}
  };
}

class LevitationDriverImpl {
  RunContext &Context;
  DependenciesStringsPool &Strings;

  Failable &Status;
  log::Logger &Log;
  TasksManager &TM;

public:

  explicit LevitationDriverImpl(RunContext &context)
  : Context(context),
    Strings(CreatableSingleton<DependenciesStringsPool>::get()),
    Status(context.Status),
    Log(log::Logger::get()),
    TM(TasksManager::get())
  {}

  void buildPreamble();
  void runParse();
  void runParseImport();
  void solveDependencies();
  void instantiateAndCodeGen();
  void codeGen();
  void runLinker();

  void collectSources();

private:

  void addMainFileInfo();

  bool processDependencyNodeDeprecated(
      const DependenciesGraph::Node &N
  );

  /// Dependency node processing
  /// \param N node to be processed
  /// \return true is successful
  bool processDependencyNode(
      const DependenciesGraph::Node &N
  );

  bool processDefinition(const DependenciesGraph::Node &N);

  bool processDeclaration(
      HashRef ExistingMeta,
      const DependenciesGraph::Node &N
  );

  bool isUpToDate(DeclASTMeta &Meta, const DependenciesGraph::Node &N);

  bool isUpToDate(
    DeclASTMeta &Meta,
    StringRef ProductFile,
    StringRef MetaFile,
    StringRef SourceFile,
    StringRef ItemDescr
  );

  void setPreambleUpdated();
  void setNodeUpdated(DependenciesGraph::NodeID::Type NID);
  void setObjectsUpdated();

  const FilesInfo& getFilesInfoFor(
      const DependenciesGraph::Node &N
  ) const;

  Paths getFullDependencies(
      const DependenciesGraph::Node &N,
      const DependenciesGraph &Graph
  ) const;

  Paths getIncludes(
      const DependenciesGraph::Node &N,
      const DependenciesGraph &Graph
  ) const;

};

/*static*/
class ArgsUtils {

  enum class QuoteType {
      None,
      SingleQuote,
      DoubleQuote
  };

  class ArgsBuilder {
    StringRef ArgsString;
    LevitationDriver::Args Args;

    std::size_t ArgStart = 0;
    std::size_t ArgEnd = 0;
    llvm::SmallVector<size_t, 16> ArgEscapes;

    QuoteType QuoteOpened = QuoteType::None;

    bool EscapeOn = false;

    bool isCurArgEmpty() const {
      return ArgStart == ArgEnd;
    }

    bool isQuoteOpened() const {
      return QuoteOpened != QuoteType::None;
    }

    void newStartPos() {
      ++ArgEnd;
      ArgStart = ArgEnd;
    }

    void addSymbol() {
      ++ArgEnd;
    }

    void skipSymbolAsEscape() {
      auto CurSymbol = ArgEnd++;
      ArgEscapes.push_back(CurSymbol);
    }

    void commitArg() {

      if (isCurArgEmpty()) {
        newStartPos();
        return;
      }

      if (ArgEscapes.empty()) {
        auto Arg = ArgsString.substr(ArgStart, ArgEnd - ArgStart);
        Args.push_back(Arg);
        return;
      }

      levitation::StringBuilder sb;
      sb << ArgsString.substr(ArgStart, ArgEscapes[0] - ArgStart);
      size_t e = ArgEscapes.size();

      for (size_t i = 1; i != e; ++i) {
        size_t Start = ArgEscapes[i-1] + 1;
        size_t End = ArgEscapes[i];
        sb << ArgsString.substr(Start, End - Start);
      }

      size_t Start = ArgEscapes[e-1] + 1;
      size_t End = ArgEnd;
      sb << ArgsString.substr(Start, End - Start);

      Args.emplace_back(std::move(sb.str()));
    }

  public:

    ArgsBuilder(StringRef argsString)
    : ArgsString(argsString)
    {}

    ~ArgsBuilder() {}

    void onQuote(QuoteType quoteType) {
      if (!isQuoteOpened()) {

        QuoteOpened = quoteType;
        onRegularSymbol();
        return;

      } if (QuoteOpened == quoteType) {
        QuoteOpened = QuoteType::None;

        // In case if quote symbol will turned out to be last
        // one before space, it will be truncated
        addSymbol();
      }
    }

    void onEscape() {
      if (EscapeOn) {
        addSymbol();
        EscapeOn = false;
        return;
      }

      EscapeOn = true;
      skipSymbolAsEscape();
    }

    void onRegularSymbol() {
      addSymbol();
    }

    void onSpace() {
      if (QuoteOpened != QuoteType::None || EscapeOn) {
        addSymbol();
        return;
      }

      commitArg();

      newStartPos();
    }

    void detachArgsTo(LevitationDriver::Args &Dest) {
      commitArg();
      Dest.swap(Args);
    }
  };

  static StringRef stripBoundingQuotesIfPresent(StringRef S) {
    size_t e = S.size();
    if (e < 2)
      return S;

    if (S[0] == S[e-1] && (S[0] == '\'' || S[0] == '"'))
      return S.substr(1, e-2);

    return S;
  }

public:
  static LevitationDriver::Args parse(StringRef S) {
    ArgsBuilder Builder(S);

    for (unsigned i = 0, e = S.size(); i != e; ++i) {
      char Symbol = S[i];

      switch (Symbol) {
        case '"':
          Builder.onQuote(QuoteType::DoubleQuote);
          break;

        case '\'':
          Builder.onQuote(QuoteType::SingleQuote);
          break;

        case '\\':
          Builder.onEscape();
          break;

        case ' ':
          Builder.onSpace();
          break;

        default:
          Builder.onRegularSymbol();
      }
    }

    LevitationDriver::Args Res;
    Builder.detachArgsTo(Res);
    return Res;
  }

  static SmallVector<StringRef, 16> toStringRefArgs(
      const LevitationDriver::Args &InputArgs
  ) {
    SmallVector<StringRef, 16> Args;
    Args.reserve(InputArgs.size());

    for (const auto &A : InputArgs) {
      auto AStripped = stripBoundingQuotesIfPresent(A);
      Args.push_back(AStripped);
    }

    return Args;
  }

  static void dump(
      llvm::raw_ostream& Out,
      const LevitationDriver::Args &Args
  ) {
    if (Args.empty())
      return;

    for (unsigned i = 0, e = Args.size(); i != e; ++i) {
      if (i != 0)
        Out << " ";
      Out << Args[i];
    }
  }
};

/*static*/
class Commands {
public:
  class CommandInfo {
    using Args = LevitationDriver::Args;
    llvm::SmallVector<SmallString<256>, 8> OwnArgs;
    SinglePath ExecutablePath;
    Args CommandArgs;

    bool Condition = true;
    bool Verbose;
    bool DryRun;

    CommandInfo(
        SinglePath &&executablePath,
        bool verbose,
        bool dryRun
    )
    : ExecutablePath(std::move(executablePath)),
      Verbose(verbose),
      DryRun(dryRun)
    {
      CommandArgs.push_back(ExecutablePath);
    }

public:
    CommandInfo() = delete;

    StringRef getExecutablePath() const {
      return ExecutablePath;
    }

    const Args &getCommandArgs() const {
      return CommandArgs;
    }

    static CommandInfo getBuildPreamble(
        StringRef BinDir,
        StringRef StdLib,
        bool verbose,
        bool dryRun
    ) {
      auto Cmd = getClangXXCommand(
          BinDir, StdLib, verbose, dryRun
      );

      Cmd
      .addArg("-cppl-preamble");

      return Cmd;
    }

    // TODO Levitation: Deprecated
    static CommandInfo getParse(
        StringRef BinDir,
        StringRef PrecompiledPreamble,
        bool verbose,
        bool dryRun
    ) {
      auto Cmd = getClangXXCommand(
          BinDir, "-libstdc++", verbose, dryRun
      );

      Cmd.addArg("-cppl-parse");

      if (PrecompiledPreamble.size())
        Cmd.addKVArgEq("-cppl-include-preamble", PrecompiledPreamble);

      return Cmd;
    }
    static CommandInfo getParseImport(
        StringRef BinDir,
        StringRef PrecompiledPreamble,
        bool verbose,
        bool dryRun
    ) {

      auto Cmd = getClangXXCommand(
          BinDir, "", verbose, dryRun
      );

      Cmd.addArg("-cppl-import");

      if (PrecompiledPreamble.size())
        Cmd.addKVArgEq("-cppl-include-preamble", PrecompiledPreamble);

      return Cmd;
    }
    static CommandInfo getInstDecl(
        StringRef BinDir,
        StringRef PrecompiledPreamble,
        bool verbose,
        bool dryRun
    ) {
      auto Cmd = getClangXXCommand(BinDir, "", verbose, dryRun);
      Cmd
      .addArg("-cppl-inst-decl");
      return Cmd;
    }

    static CommandInfo getInstObj(
        StringRef BinDir,
        StringRef PrecompiledPreamble,
        bool verbose,
        bool dryRun
    ) {
      auto Cmd = getClangXXCommand(BinDir, "", verbose, dryRun);
      Cmd
      .addArg("-cppl-compile");
      return Cmd;
    }

    static CommandInfo getBuildDecl(
        StringRef BinDir,
        StringRef PrecompiledPreamble,
        StringRef StdLib,
        bool verbose,
        bool dryRun
    ) {
      auto Cmd = getClangXXCommand(BinDir, StdLib, verbose, dryRun);
      Cmd
      .addArg("-cppl-decl");
      return Cmd;
    }

    static CommandInfo getBuildObj(
        StringRef BinDir,
        StringRef PrecompiledPreamble,
        StringRef StdLib,
        bool verbose,
        bool dryRun
    ) {
      auto Cmd = getClangXXCommand(BinDir, StdLib, verbose, dryRun);
      Cmd
      .addArg("-cppl-obj");
      return Cmd;
    }

    static CommandInfo getCompileSrc(
        StringRef BinDir,
        StringRef PrecompiledPreamble,
        bool verbose,
        bool dryRun
    ) {
      auto Cmd = getClangXXCommand(BinDir, "", verbose, dryRun);

      Cmd.addArg("-cppl-compile");

      if (PrecompiledPreamble.size())
        Cmd.addKVArgEq("-cppl-include-preamble", PrecompiledPreamble);

      return Cmd;
    }
    static CommandInfo getLink(
        StringRef BinDir,
        StringRef StdLib,
        bool verbose,
        bool dryRun,
        bool CanUseLibStdCpp
    ) {
      CommandInfo Cmd(getClangXXPath(BinDir), verbose, dryRun);

      if (!CanUseLibStdCpp)
        Cmd.addArg("-stdlib=libc++");
      else
        Cmd.addKVArgEqIfNotEmpty("-stdlib", StdLib);

      return Cmd;
    }

    CommandInfo& addArg(StringRef Arg) {
      if (!Condition) return *this;
      CommandArgs.emplace_back(Arg);
      return *this;
    }

    CommandInfo& addKVArgSpace(StringRef Arg, StringRef Value) {
      if (!Condition) return *this;
      CommandArgs.emplace_back(Arg);
      CommandArgs.emplace_back(Value);
      return *this;
    }

    CommandInfo& addKVArgEq(StringRef Arg, StringRef Value) {
      if (!Condition) return *this;
      OwnArgs.emplace_back((Arg + "=" + Value).str());
      CommandArgs.emplace_back(OwnArgs.back());
      return *this;
    }

    CommandInfo& addKVArgEqIfNotEmpty(StringRef Arg, StringRef Value) {
      if (!Condition) return *this;
      if (Value.size())
        addKVArgEq(Arg, Value);
      return *this;
    }

    template <typename ValuesT>
    CommandInfo& addArgs(const ValuesT& Values) {
      if (!Condition) return *this;
      for (const auto &Value : Values) {
        CommandArgs.emplace_back(Value);
      }
      return *this;
    }

    template <typename ValuesT>
    CommandInfo& addKVArgsEq(StringRef Name, const ValuesT Values) {
      if (!Condition) return *this;
      for (const auto &Value : Values) {
        OwnArgs.emplace_back((Name + "=" + Value).str());
        CommandArgs.emplace_back(OwnArgs.back());
      }
      return *this;
    }

    CommandInfo& condition(bool Value) {
      Condition = Value;
      return *this;
    }

    CommandInfo& conditionElse() {
      Condition = !Condition;
      return *this;
    }

    CommandInfo& conditionEnd() {
      Condition = true;
      return *this;
    }

    Failable execute() {
      if (DryRun || Verbose) {
        dumpCommand();
      }

      if (!DryRun) {
        std::string ErrorMessage;

        auto Args = ArgsUtils::toStringRefArgs(CommandArgs);

        int Res = llvm::sys::ExecuteAndWait(
            ExecutablePath,
            Args,
            /*Env*/llvm::None,
            /*Redirects*/{},
            /*secondsToWait*/ 0,
            /*memoryLimit*/ 0,
            &ErrorMessage,
            /*ExectutionFailed*/nullptr
        );

        Failable Status;

        if (Res != 0) {
          Status.setFailure() << ErrorMessage;
        } else if (ErrorMessage.size()) {
          Status.setWarning() << ErrorMessage;
        }

        return Status;
      }

      return Failable();
    }
  protected:

    static SinglePath getClangPath(llvm::StringRef BinDir) {

      const char *ClangBin = "clang";

      if (BinDir.size()) {
        SinglePath P = BinDir;
        llvm::sys::path::append(P, ClangBin);
        return P;
      }

      return SinglePath(ClangBin);
    }

    static SinglePath getClangXXPath(llvm::StringRef BinDir) {

      const char *ClangBin = "clang++";

      if (BinDir.size()) {
        SinglePath P = BinDir;
        llvm::sys::path::append(P, ClangBin);
        return P;
      }

      return SinglePath(ClangBin);
    }

    static CommandInfo getClangXXCommand(
        llvm::StringRef BinDir,
        StringRef StdLib,
        bool verbose,
        bool dryRun
    ) {
      CommandInfo Cmd(getClangXXPath(BinDir), verbose, dryRun);
      Cmd
      .addArg("-std=c++17")
      .addKVArgEqIfNotEmpty("-stdlib", StdLib);

      return Cmd;
    }

    static CommandInfo getBase(
        llvm::StringRef BinDir,
        llvm::StringRef PrecompiledPreamble,
        bool verbose,
        bool dryRun
    ) {
      CommandInfo Cmd(getClangPath(BinDir), verbose, dryRun);
      Cmd.setupCCFlags();

      if (PrecompiledPreamble.size())
        Cmd.addKVArgEq("-levitation-preamble", PrecompiledPreamble);

      return Cmd;
    }

    void setupCCFlags() {
       addArg("-cc1")
      .addArg("-std=c++17")
      .addArg("-stdlib=libstdc++");
    }

    void dumpCommand() {

      auto &Out = log::Logger::get().info();

      for (unsigned i = 0, e = CommandArgs.size(); i != e; ++i) {
        if (i != 0)
          Out << " ";
        Out << CommandArgs[i];
      }

      Out << "\n";
    }
  };

  static bool parse(
      StringRef BinDir,
      StringRef PrecompiledPreamble,
      StringRef OutASTFile,
      StringRef OutLDepsFile,
      StringRef SourceFile,
      StringRef SourcesRoot,
      const LevitationDriver::Args &ExtraArgs,
      bool Verbose,
      bool DryRun
  ) {
    if (!DryRun || Verbose)
      dumpParse(OutASTFile, OutLDepsFile, SourceFile);

    levitation::Path::createDirsForFile(OutASTFile);
    levitation::Path::createDirsForFile(OutLDepsFile);

    auto ExecutionStatus = CommandInfo::getParse(
        BinDir, PrecompiledPreamble, Verbose, DryRun
    )
    .addKVArgEq("-cppl-src-root", SourcesRoot)
    .addKVArgEq("-cppl-deps-out", OutLDepsFile)
    .addArgs(ExtraArgs)
    .addArg(SourceFile)
    .addKVArgSpace("-o", OutASTFile)
    .execute();

    return processStatus(ExecutionStatus);
  }

  static bool parseImport(
      StringRef BinDir,
      StringRef PrecompiledPreamble,
      StringRef OutLDepsFile,
      StringRef OutLDepsMetaFile,
      StringRef SourceFile,
      StringRef SourcesRoot,
      const LevitationDriver::Args &ExtraArgs,
      bool Verbose,
      bool DryRun
  ) {
    if (!DryRun || Verbose)
      dumpParseImport(OutLDepsFile, SourceFile);

    levitation::Path::createDirsForFile(OutLDepsFile);

    auto ExecutionStatus = CommandInfo::getParseImport(
        BinDir, PrecompiledPreamble, Verbose, DryRun
    )
    .addKVArgEq("-cppl-src-root", SourcesRoot)
    .addKVArgEq("-cppl-deps-out", OutLDepsFile)
    .addKVArgEq("-cppl-meta", OutLDepsMetaFile)
    .addArgs(ExtraArgs)
    .addArg(SourceFile)
    .execute();

    return processStatus(ExecutionStatus);
  }

  static bool instantiateDecl(
      StringRef BinDir,
      StringRef PrecompiledPreamble,
      StringRef OutDeclASTFile,
      StringRef InputObject,
      const Paths &Deps,
      const LevitationDriver::Args &ExtraArgs,
      bool Verbose,
      bool DryRun
  ) {
    assert(OutDeclASTFile.size() && InputObject.size());

    if (!DryRun || Verbose)
      dumpInstantiateDecl(OutDeclASTFile, InputObject, Deps);

    levitation::Path::createDirsForFile(OutDeclASTFile);

    auto ExecutionStatus = CommandInfo::getInstDecl(
        BinDir, PrecompiledPreamble, Verbose, DryRun
    )
    .addKVArgEqIfNotEmpty("-cppl-include-preamble", PrecompiledPreamble)
    .addKVArgsEq("-cppl-include-dependency", Deps)
    .addArgs(ExtraArgs)
    .addArg(InputObject)
    .addKVArgSpace("-o", OutDeclASTFile)
    .execute();

    return processStatus(ExecutionStatus);
  }

  static bool instantiateObject(
      StringRef BinDir,
      StringRef PrecompiledPreamble,
      StringRef OutObjFile,
      StringRef InputObject,
      const Paths &Deps,
      const LevitationDriver::Args &ExtraArgs,
      bool Verbose,
      bool DryRun
  ) {
    assert(OutObjFile.size() && InputObject.size());

    if (!DryRun || Verbose)
      dumpInstantiateObject(OutObjFile, InputObject, Deps);

    levitation::Path::createDirsForFile(OutObjFile);

    auto ExecutionStatus = CommandInfo::getInstObj(
        BinDir, PrecompiledPreamble, Verbose, DryRun
    )
    .addKVArgEqIfNotEmpty("-cppl-include-preamble", PrecompiledPreamble)
    .addKVArgsEq("-cppl-include-dependency", Deps)
    .addArgs(ExtraArgs)
    .addArg(InputObject)
    .addKVArgSpace("-o", OutObjFile)
    .execute();

    return processStatus(ExecutionStatus);
  }

  static bool buildDecl(
      StringRef BinDir,
      StringRef PrecompiledPreamble,
      StringRef OutDeclASTFile,
      StringRef OutDeflASTMetaFile,
      StringRef InputFile,
      const Paths &Deps,
      StringRef StdLib,
      const LevitationDriver::Args &ExtraParserArgs,
      bool Verbose,
      bool DryRun
  ) {
    assert(OutDeclASTFile.size() && InputFile.size());

    if (!DryRun || Verbose)
      dumpBuildDecl(OutDeclASTFile, OutDeflASTMetaFile, InputFile, Deps);

    levitation::Path::createDirsForFile(OutDeclASTFile);

    auto ExecutionStatus = CommandInfo::getBuildDecl(
        BinDir, PrecompiledPreamble, StdLib, Verbose, DryRun
    )
    .addKVArgEqIfNotEmpty("-cppl-include-preamble", PrecompiledPreamble)
    .addKVArgsEq("-cppl-include-dependency", Deps)
    .addArgs(ExtraParserArgs)
    .addArg(InputFile)
    // TODO Levitation: don't emit .decl-ast files
    //  in some cases. See task #48
    //    .condition(OutDeclASTFile.size())
    //        .addKVArgSpace("-o", OutDeclASTFile)
    //    .conditionElse()
    //        .addArg("-cppl-no-out")
    //    .conditionEnd()
    .addKVArgSpace("-o", OutDeclASTFile)
    .addKVArgEq("-cppl-meta", OutDeflASTMetaFile)
    .execute();

    return processStatus(ExecutionStatus);
  }

  static bool buildObject(
      StringRef BinDir,
      StringRef PrecompiledPreamble,
      StringRef OutObjFile,
      StringRef OutMetaFile,
      StringRef InputObject,
      const Paths &Deps,
      StringRef StdLib,
      const LevitationDriver::Args &ExtraParserArgs,
      const LevitationDriver::Args &ExtraCodeGenArgs,
      bool Verbose,
      bool DryRun
  ) {
    assert(OutObjFile.size() && InputObject.size());

    if (!DryRun || Verbose)
      dumpBuildObject(OutObjFile, InputObject, Deps);

    levitation::Path::createDirsForFile(OutObjFile);

    auto ExecutionStatus = CommandInfo::getBuildObj(
        BinDir, PrecompiledPreamble, StdLib, Verbose, DryRun
    )
    .addKVArgEqIfNotEmpty("-cppl-include-preamble", PrecompiledPreamble)
    .addKVArgsEq("-cppl-include-dependency", Deps)
    .addArgs(ExtraParserArgs)
    .addArgs(ExtraCodeGenArgs)
    .addArg(InputObject)
    .addKVArgSpace("-o", OutObjFile)
    .addKVArgEq("-cppl-meta", OutMetaFile)
    .execute();

    return processStatus(ExecutionStatus);
  }

  static bool buildPreamble(
      StringRef BinDir,
      StringRef PreambleSource,
      StringRef PCHOutput,
      StringRef PCHOutputMeta,
      StringRef StdLib,
      const LevitationDriver::Args &ExtraPreambleArgs,
      bool Verbose,
      bool DryRun
  ) {
    assert(PreambleSource.size() && PCHOutput.size());

    if (!DryRun || Verbose)
      dumpBuildPreamble(PreambleSource, PCHOutput);

    levitation::Path::createDirsForFile(PCHOutput);

    auto ExecutionStatus = CommandInfo::getBuildPreamble(
        BinDir, StdLib, Verbose, DryRun
    )
    .addArg(PreambleSource)
    .addKVArgSpace("-o", PCHOutput)
    .addKVArgEq("-cppl-meta", PCHOutputMeta)
    .addArgs(ExtraPreambleArgs)
    .execute();

    return processStatus(ExecutionStatus);
  }

  static bool link(
      StringRef BinDir,
      StringRef OutputFile,
      const Paths &ObjectFiles,
      StringRef StdLib,
      const LevitationDriver::Args &ExtraArgs,
      bool Verbose,
      bool DryRun,
      bool CanUseLibStdCpp
  ) {
    assert(OutputFile.size() && ObjectFiles.size());

    if (!DryRun || Verbose)
      dumpLink(OutputFile, ObjectFiles);

    levitation::Path::createDirsForFile(OutputFile);

    auto ExecutionStatus = CommandInfo::getLink(
        BinDir, StdLib, Verbose, DryRun, CanUseLibStdCpp
    )
    .addArgs(ExtraArgs)
    .addArgs(ObjectFiles)
    .addKVArgSpace("-o", OutputFile)
    .execute();

    return true;
  }

protected:

  static log::manipulator_t workerId() {
    auto &TM = TasksManager::get();
    auto WID = TM.getWorkerID();
      if (TasksManager::isValid(WID))
        return [=] (llvm::raw_ostream &out) {
          out << WID;
        };
      else
        return [=] (llvm::raw_ostream &out) {
          out << "Main";
        };
  }

  template <typename ...ArgsT>
  static void log_info(ArgsT&&...args) {
    log::Logger::get().log_info(
        "[", workerId(), "] ",
        std::forward<ArgsT>(args)...
    );
  }

  template <typename ...ArgsT>
  static void log_verbose(ArgsT&&...args) {
    log::Logger::get().log_verbose(
        "[", workerId(), "] ",
        std::forward<ArgsT>(args)...
    );
  }

  static void dumpBuildPreamble(
      StringRef PreambleSource,
      StringRef PreambleOut
  ) {
    log_info(
        "PREAMBLE ", PreambleSource, " -> ",
        "preamble out: ", PreambleOut
    );
  }

  // TODO: Deprecarted
  static void dumpParse(
      StringRef OutASTFile,
      StringRef OutLDepsFile,
      StringRef SourceFile
  ) {
    auto &LogInfo = log::Logger::get().info();
    LogInfo
    << "PARSE     " << SourceFile << " -> "
    << "(ast:" << OutASTFile << ", "
    << "ldeps: " << OutLDepsFile << ")"
    << "\n";
  }

  static void dumpParseImport(
      StringRef OutLDepsFile,
      StringRef SourceFile
  ) {
    log_info(
        "PARSE IMP ", SourceFile, " -> ",
        "(ldeps: ", OutLDepsFile, ")"
    );
  }


  static void dumpInstantiateDecl(
      StringRef OutDeclASTFile,
      StringRef InputObject,
      const Paths &Deps
  ) {
    assert(OutDeclASTFile.size() && InputObject.size());

    dumpInstantiate(OutDeclASTFile, InputObject, Deps, "INST DECL", "decl-ast");
  }

  static void dumpBuildDecl(
      StringRef OutDeclASTFile,
      StringRef OutDeclASTMetaFile,
      StringRef InputObject,
      const Paths &Deps
  ) {
    assert(OutDeclASTFile.size() && InputObject.size());

    // TODO Levitation: also dump OutDeclASTMetaFile

    dumpInstantiate(OutDeclASTFile, InputObject, Deps, "BUILD DECL", "decl-ast");
  }

  static void dumpBuildObject(
      StringRef OutObjFile,
      StringRef InputObject,
      const Paths &Deps
  ) {
    assert(OutObjFile.size() && InputObject.size());

    dumpInstantiate(OutObjFile, InputObject, Deps, "BUILD OBJ ", "object");
  }

  static void dumpInstantiateObject(
      StringRef OutObjFile,
      StringRef InputObject,
      const Paths &Deps
  ) {
    assert(OutObjFile.size() && InputObject.size());

    dumpInstantiate(OutObjFile, InputObject, Deps, "INST OBJ ", "object");
  }

  static void dumpCompileMain(
      StringRef OutObjFile,
      StringRef InputObject,
      const Paths &Deps
  ) {
    assert(OutObjFile.size() && InputObject.size());

    dumpInstantiate(OutObjFile, InputObject, Deps, "MAIN OBJ ", "object");
  }

  static void dumpInstantiate(
      StringRef OutDeclASTFile,
      StringRef InputObject,
      const Paths &Deps,
      StringRef ActionName,
      StringRef OutputName
  ) {
    assert(OutDeclASTFile.size() && InputObject.size());

    log_info(
        ActionName, " ", InputObject,
        ", ",
        dumpLDepsFiles(Deps),
        " -> ", OutputName, ": ", OutDeclASTFile
    );
  }

  static log::manipulator_t dumpLDepsFiles(
      const Paths &Deps
  ) {
    return dumpPathsArray(Deps, "deps");
  }

  static void dumpLink(StringRef OutputFile, const Paths &ObjectFiles) {
    assert(OutputFile.size() && ObjectFiles.size());
    log_info(
        "LINK ",
        dumpObjectFiles(ObjectFiles),
        " -> ", OutputFile
    );
  }

  static log::manipulator_t dumpObjectFiles(
      const Paths &ObjectFiles
  ) {
    return dumpPathsArray(ObjectFiles, "objects");
  }

  static log::manipulator_t dumpPathsArray(
      const Paths &ObjectFiles,
      StringRef ArrayName
  ) {
    return [=] (llvm::raw_ostream &out) {
      out << ArrayName << ": ";

      if (ObjectFiles.size()) {
        out << "(";
        for (size_t i = 0, e = ObjectFiles.size(); i != e; ++i) {
          out << ObjectFiles[i];
          if (i + 1 != e)
            out << ", ";
        }
        out << ")";
      } else {
        out << "<empty>";
      }
    };
  }

  static bool processStatus(const Failable &Status) {
    if (Status.hasWarnings())
      log::Logger::get().warning() << Status.getWarningMessage();

    if (!Status.isValid()) {
      log::Logger::get().error() << Status.getErrorMessage();
      return false;
    }

    return true;
  }
};

void LevitationDriverImpl::buildPreamble() {
  if (!Status.isValid())
    return;

  if (!Context.Driver.isPreambleCompilationRequested())
    return;

  if (Context.Driver.PreambleOutput.empty()) {
    Context.Driver.PreambleOutput = levitation::Path::getPath<SinglePath>(
      Context.Driver.BuildRoot,
      DriverDefaults::PREAMBLE_OUT
    );
    Context.Driver.PreambleOutputMeta = levitation::Path::getPath<SinglePath>(
      Context.Driver.BuildRoot,
      DriverDefaults::PREAMBLE_OUT_META
    );
  }

  DeclASTMeta Meta;
  if (isUpToDate(
      Meta,
      Context.Driver.PreambleOutput,
      Context.Driver.PreambleOutputMeta,
      Context.Driver.PreambleSource,
      Context.Driver.PreambleSource
  ))
    return;

  auto Res = Commands::buildPreamble(
    Context.Driver.BinDir,
    Context.Driver.PreambleSource,
    Context.Driver.PreambleOutput,
    Context.Driver.PreambleOutputMeta,
    Context.Driver.StdLib,
    Context.Driver.ExtraPreambleArgs,
    Context.Driver.Verbose,
    Context.Driver.DryRun
  );

  if (!Res)
    Status.setFailure()
    << "Preamble: phase failed";

  setPreambleUpdated();
}

// TODO Levitation: deprecated
void LevitationDriverImpl::runParse() {
  auto &TM = TasksManager::get();

  for (auto PackagePath : Context.Packages) {

    auto Files = Context.Files[PackagePath];

    TM.addTask([=] (TasksManager::TaskContext &TC) {
      TC.Successful = Commands::parse(
          Context.Driver.BinDir,
          Context.Driver.PreambleOutput,
          Files.AST,
          Files.LDeps,
          Files.Source,
          Context.Driver.SourcesRoot,
          Context.Driver.ExtraParseArgs,
          Context.Driver.Verbose,
          Context.Driver.DryRun
      );
    });
  }

  auto Res = TM.waitForTasks();

  if (!Res)
    Status.setFailure()
    << "Parse: phase failed.";
}

void LevitationDriverImpl::runParseImport() {
  auto &TM = TasksManager::get();

  for (auto PackagePath : Context.Packages) {

    auto Files = Context.Files[PackagePath];

    DeclASTMeta ldepsMeta;

    if (isUpToDate(
        ldepsMeta,
        Files.LDeps,
        Files.LDepsMeta,
        Files.Source,
        Files.LDeps /* item description */
    ))
      continue;

    TM.addTask([=] (TasksManager::TaskContext &TC) {
      TC.Successful = Commands::parseImport(
          Context.Driver.BinDir,
          Context.Driver.PreambleOutput,
          Files.LDeps,
          Files.LDepsMeta,
          Files.Source,
          Context.Driver.SourcesRoot,
          Context.Driver.ExtraParseImportArgs,
          Context.Driver.Verbose,
          Context.Driver.DryRun
      );
    });
  }

  auto Res = TM.waitForTasks();

  if (!Res)
    Status.setFailure()
    << "Parse: phase failed.";
}

void LevitationDriverImpl::solveDependencies() {
  if (!Status.isValid())
    return;

  DependenciesSolver Solver;
  Solver.setSourcesRoot(Context.Driver.SourcesRoot);
  Solver.setBuildRoot(Context.Driver.BuildRoot);
  Solver.setVerbose(Context.Driver.Verbose);

  Paths LDepsFiles;
  for (auto &PackagePath : Context.Packages) {
    assert(Context.Files.count(PackagePath));
    LDepsFiles.push_back(Context.Files[PackagePath].LDeps);
  }

  Context.DependenciesInfo = Solver.solve(LDepsFiles);

  Status.inheritResult(Solver, "Dependencies solver: ");
}

// TODO Levitation: deprecated
void LevitationDriverImpl::instantiateAndCodeGen() {
  if (!Status.isValid())
    return;

  bool Res =
    Context.DependenciesInfo->getDependenciesGraph().dsfJobs(
        [&] (const DependenciesGraph::Node &N) {
          return processDependencyNodeDeprecated(N);
        }
    );

  if (!Res)
    Status.setFailure()
    << "Instantiate and codegen: phase failed.";
}

void LevitationDriverImpl::codeGen() {
  if (!Status.isValid())
    return;

  bool Res =
    Context.DependenciesInfo->getDependenciesGraph().dsfJobs(
        [&] (const DependenciesGraph::Node &N) {
          return processDependencyNode(N);
        }
    );

  if (!Res)
    Status.setFailure()
    << "Instantiate and codegen: phase failed.";
}

void LevitationDriverImpl::runLinker() {
  if (!Status.isValid())
    return;

  if (llvm::sys::fs::exists(Context.Driver.Output) && !Context.ObjectsUpdated) {
    Status.setWarning("Nothing to build.\n");
    return;
  }

  assert(Context.Driver.isLinkPhaseEnabled() && "Link phase must be enabled.");

  Paths ObjectFiles;
  for (auto &PackagePath : Context.Packages) {
    assert(Context.Files.count(PackagePath));
    ObjectFiles.push_back(Context.Files[PackagePath].Object);
  }

  auto Res = Commands::link(
      Context.Driver.BinDir,
      Context.Driver.Output,
      ObjectFiles,
      Context.Driver.StdLib,
      Context.Driver.ExtraLinkerArgs,
      Context.Driver.Verbose,
      Context.Driver.DryRun,
      Context.Driver.CanUseLibStdCppForLinker
  );

  if (!Res)
    Status.setFailure()
    << "Link: phase failed";
}

void LevitationDriverImpl::collectSources() {

  Log.verbose() << "Collecting sources...\n";

  // Gather all .cppl files
  FileSystem::collectFiles(
      Context.Packages,
      Context.Driver.SourcesRoot,
      FileExtensions::SourceCode
  );

  // Normalize all paths to .cppl files
  for (auto &Src : Context.Packages) {
    Src = levitation::Path::makeRelative<SinglePath>(
        Src, Context.Driver.SourcesRoot
    );
  }

  for (const auto &PackagePath : Context.Packages) {

    FilesInfo Files;

    // In current implementation package path is equal to relative source path.

    Files.Source = Path::getPath<SinglePath>(
        Context.Driver.SourcesRoot,
        PackagePath,
        FileExtensions::SourceCode
    );

    Files.Header = Path::getPath<SinglePath>(
        Context.Driver.getOutputHeadersDir(),
        PackagePath,
        FileExtensions::Header
    );

    Files.DeclASTMetaFile = Path::getPath<SinglePath>(
        Context.Driver.BuildRoot,
        PackagePath,
        FileExtensions::DeclASTMeta
    );

    Files.ObjMetaFile = Path::getPath<SinglePath>(
        Context.Driver.BuildRoot,
        PackagePath,
        FileExtensions::ObjMeta
    );

    Files.LDeps = Path::getPath<SinglePath>(
        Context.Driver.BuildRoot,
        PackagePath,
        FileExtensions::ParsedDependencies
    );

    Files.LDepsMeta = Path::getPath<SinglePath>(
        Context.Driver.BuildRoot,
        PackagePath,
        FileExtensions::ParsedDependenciesMeta
    );

    Files.DeclAST = Path::getPath<SinglePath>(
        Context.Driver.BuildRoot,
        PackagePath,
        FileExtensions::DeclarationAST
    );
    Files.Object = Path::getPath<SinglePath>(
        Context.Driver.BuildRoot,
        PackagePath,
        FileExtensions::Object
    );

    auto Res = Context.Files.insert({ PackagePath, Files });

    assert(Res.second);
  }

  Log.verbose()
  << "Found " << Context.Packages.size()
  << " '." << FileExtensions::SourceCode << "' files.\n\n";
}

// TODO Levitation: try to make this method const.
bool LevitationDriverImpl::processDependencyNode(
    const DependenciesGraph::Node &N
) {
  DeclASTMeta ExistingMeta;
  if (isUpToDate(ExistingMeta, N))
    return true;

  switch (N.Kind) {
    case DependenciesGraph::NodeKind::Declaration:
      return processDeclaration(ExistingMeta.getDeclASTHash(), N);
    case DependenciesGraph::NodeKind::Definition: {
      return processDefinition(N);
    }
    default:
      llvm_unreachable("Unknown dependency kind");
  }
}

const FilesInfo& LevitationDriverImpl::getFilesInfoFor(
    const DependenciesGraph::Node &N
) const {
  const auto &SrcRel = *Strings.getItem(N.PackageInfo->PackagePath);

  auto FoundFiles = Context.Files.find(SrcRel);
  if(FoundFiles == Context.Files.end()) {
    Log.error()
    << "Package '" << SrcRel << "' is present in dependencies, but not found.\n";
    llvm_unreachable("Package not found");
  }

  return FoundFiles->second;
}

Paths LevitationDriverImpl::getFullDependencies(
    const DependenciesGraph::Node &N,
    const DependenciesGraph &Graph
) const {
  auto &FullDepsRanged = Context.DependenciesInfo->getRangedDependencies(N.ID);

  Paths FullDeps;
  for (auto RangeNID : FullDepsRanged) {
    auto &DNode = Graph.getNode(RangeNID.second);
    auto DepPath = *Strings.getItem(DNode.PackageInfo->PackagePath);

    DependenciesSolverPath::addDepPathsFor(
        FullDeps,
        Context.Driver.BuildRoot,
        DepPath
    );
  }
  return FullDeps;
}

Paths LevitationDriverImpl::getIncludes(
    const DependenciesGraph::Node &N,
    const DependenciesGraph &Graph
) const {
  Paths Includes;
  for (auto DepNID : N.Dependencies) {
    auto &DNode = Graph.getNode(DepNID);
    auto DepPath = *Strings.getItem(DNode.PackageInfo->PackagePath);

    DependenciesSolverPath::addIncPathsFor(
        Includes,
        Context.Driver.BuildRoot,
        DepPath
    );
  }
  return Includes;
}

bool LevitationDriverImpl::processDefinition(
    const DependenciesGraph::Node &N
) {
  assert(
      N.Kind == DependenciesGraph::NodeKind::Definition &&
      "Only definition nodes expected here"
  );

  const auto &Graph = Context.DependenciesInfo->getDependenciesGraph();
  const auto &Files = getFilesInfoFor(N);

  Paths fullDependencies = getFullDependencies(N, Graph);

  setObjectsUpdated();

  return Commands::buildObject(
    Context.Driver.BinDir,
    Context.Driver.PreambleOutput,
    Files.Object,
    Files.ObjMetaFile,
    Files.Source,
    fullDependencies,
    Context.Driver.StdLib,
    Context.Driver.ExtraParseArgs,
    Context.Driver.ExtraCodeGenArgs,
    Context.Driver.Verbose,
    Context.Driver.DryRun
  );
}

bool LevitationDriverImpl::processDeclaration(
    HashRef OldDeclASTHash,
    const DependenciesGraph::Node &N
) {
  const auto &Graph = Context.DependenciesInfo->getDependenciesGraph();
  const auto &Files = getFilesInfoFor(N);
  Paths fullDependencies = getFullDependencies(N, Graph);

  bool NeedDeclAST = true;

  if (N.DependentNodes.empty() && !Graph.isPublic(N.ID)) {
    auto &Verbose = Log.verbose();
    Verbose << "TODO: Skip building unused declaration for ";
    Graph.dumpNodeShort(Verbose, N.ID, Strings);
    Verbose << "\n";

    // TODO Levitation: see #48
    // NeedDeclAST = false;
  }

  bool buildDeclSuccessfull = Commands::buildDecl(
      Context.Driver.BinDir,
      Context.Driver.PreambleOutput,
      (NeedDeclAST ? Files.DeclAST.str() : StringRef()),
      (NeedDeclAST ? Files.DeclASTMetaFile.str() : StringRef()),
      Files.Source,
      fullDependencies,
      Context.Driver.StdLib,
      Context.Driver.ExtraParseArgs,
      Context.Driver.Verbose,
      Context.Driver.DryRun
  );

  if (!buildDeclSuccessfull)
    return false;

  bool MustGenerateHeaders =
      Context.Driver.shouldCreateHeaders() &&
      Graph.isPublic(N.ID);

  auto Includes = getIncludes(N, Graph);

  DeclASTMeta Meta;
  if (!DeclASTMetaLoader::fromFile(
      Meta, Context.Driver.BuildRoot, Files.DeclASTMetaFile
  ))
    return false;

  bool Success = true;

  if (MustGenerateHeaders) {
    Success = HeaderGenerator(
        Files.Header,
        Files.Source,
        N.Dependencies.empty() ? Context.Driver.PreambleSource : "",
        Includes,
        Meta.getFragmentsToSkip(),
        Context.Driver.Verbose,
        Context.Driver.DryRun
    )
    .execute();
  }

  // Mark that node was updated, if it was updated

  if (!equal(OldDeclASTHash, Meta.getDeclASTHash()))
    setNodeUpdated(N.ID);
  else {
    auto &Verbose = Log.info();
    Verbose << "Node ";
    Graph.dumpNodeShort(Verbose, N.ID, Strings);
    Verbose << " is up-to-date.\n";
  }

  return Success;
}

bool LevitationDriverImpl::isUpToDate(
    DeclASTMeta &Meta,
    const DependenciesGraph::Node &N
) {
  if (Context.PreambleUpdated)
    return false;

  for (auto D : N.Dependencies)
    if (Context.UpdatedNodes.count(D))
      return false;

  const auto &Files = getFilesInfoFor(N);

  StringRef MetaFile;
  StringRef ProductFile;

  switch (N.Kind) {
    case DependenciesGraph::NodeKind::Declaration:
      MetaFile = Files.DeclASTMetaFile;
      ProductFile = Files.DeclAST;
      break;
    case DependenciesGraph::NodeKind::Definition:
      MetaFile = Files.ObjMetaFile;
      ProductFile = Files.Object;
      break;
    default:
      return false;
  }

  auto NodeDescr = Context.DependenciesInfo->getDependenciesGraph()
      .nodeDescrShort(N.ID, Strings);

  return isUpToDate(Meta, ProductFile, MetaFile, Files.Source, NodeDescr);
}

bool LevitationDriverImpl::isUpToDate(
    DeclASTMeta &Meta,
    llvm::StringRef ProductFile,
    llvm::StringRef MetaFile,
    llvm::StringRef SourceFile,
    llvm::StringRef ItemDescr
) {
  if (!llvm::sys::fs::exists(MetaFile))
    return false;

  if (!llvm::sys::fs::exists(ProductFile))
    return false;

  if (!DeclASTMetaLoader::fromFile(
      Meta, Context.Driver.BuildRoot, MetaFile
  )) {
    Log.warning()
    << "Failed to load existing meta file for '"
    << SourceFile << "'\n"
    << "  Must rebuild dependent chains.";
    return false;
  }

  // Get source MD5

  auto &FM = CreatableSingleton<FileManager>::get();

  if (auto Buffer = FM.getBufferForFile(SourceFile)) {
    auto SrcMD5 = calcMD5(Buffer->get()->getBuffer());

#if 0
    auto &Verbose = Log.verbose();
    Verbose << "Old Hash: ";
    for (auto b : Meta.getSourceHash()) {
      Verbose.write_hex(b);
      Verbose << " ";
    }

    Verbose << "\n";

    Verbose << "Src Hash: ";
    for (auto b : SrcMD5.Bytes) {
      Verbose.write_hex(b);
      Verbose << " ";
    }

    Verbose << "\n";
#endif

  // FIXME Levitation: we either should give up and remove this check
  //  or somehow separation md5 for source locations block
  //  and rest of decl-ast file.
  // Currently each time you change source, you change source locations.
  // So, even though declaration itself may remain same, .decl-ast
  // will be different.
  bool Res = equal(Meta.getSourceHash(), SrcMD5.Bytes);

  if (Res) {
      auto &Verbose = Log.verbose();
      Verbose << "Source  for item '" << ItemDescr << "' is up-to-date.\n";
    }
    return Res;
  } else
     Log.warning()
    << "Failed to load source '"
    << SourceFile << "' during up-to-date checks.\n"
    << "  Must rebuild dependent chains. But I think I'll fail, dude...";
    return false;
}

void LevitationDriverImpl::setPreambleUpdated() {
  Context.PreambleUpdated = true;
}

void LevitationDriverImpl::setNodeUpdated(DependenciesGraph::NodeID::Type NID) {
  Context.UpdatedNodes.insert(NID);
}


void LevitationDriverImpl::setObjectsUpdated() {
  Context.ObjectsUpdated = true;
}

// TODO Levitation: Deprecated
bool LevitationDriverImpl::processDependencyNodeDeprecated(
    const DependenciesGraph::Node &N
) {
  const auto &Strings = CreatableSingleton<DependenciesStringsPool>::get();
  const auto &Graph = Context.DependenciesInfo->getDependenciesGraph();

  const auto &SrcRel = *Strings.getItem(N.PackageInfo->PackagePath);

  auto FoundFiles = Context.Files.find(SrcRel);
  if(FoundFiles == Context.Files.end()) {
    Log.error()
    << "Package '" << SrcRel << "' is present in dependencies, but not found.\n";
    llvm_unreachable("Package not found");
  }

  const auto &Files = FoundFiles->second;

  auto &RangedDeps = Context.DependenciesInfo->getRangedDependencies(N.ID);

  Paths fullDependencies;
  for (auto RangeNID : RangedDeps) {
    auto &DNode = Graph.getNode(RangeNID.second);
    auto DepPath = *Strings.getItem(DNode.PackageInfo->PackagePath);

    DependenciesSolverPath::addDepPathsForDeprecated(
        fullDependencies,
        Context.Driver.BuildRoot,
        DepPath
    );
  }

  switch (N.Kind) {

    case DependenciesGraph::NodeKind::Declaration:
      return Commands::instantiateDecl(
          Context.Driver.BinDir,
          Context.Driver.PreambleOutput,
          Files.DeclAST,
          Files.AST,
          fullDependencies,
          Context.Driver.ExtraCodeGenArgs,
          Context.Driver.Verbose,
          Context.Driver.DryRun
      );

    case DependenciesGraph::NodeKind::Definition: {
      return Commands::instantiateObject(
        Context.Driver.BinDir,
        Context.Driver.PreambleOutput,
        Files.Object,
        Files.AST,
        fullDependencies,
        Context.Driver.ExtraCodeGenArgs,
        Context.Driver.Verbose,
        Context.Driver.DryRun
      );
    }

    default:
      llvm_unreachable("Unknown dependency kind");
  }
}

//-----------------------------------------------------------------------------
//  LevitationDriver

LevitationDriver::LevitationDriver(StringRef CommandPath)
{
  SinglePath P = CommandPath;
  if (auto Err = llvm::sys::fs::make_absolute(P)) {
    log::Logger::get().warning()
    << "Failed to make absolute path. System message: "
    << Err.message() << "\n";
    P = CommandPath;
  }

  BinDir = llvm::sys::path::parent_path(P);

  OutputHeadersDir = levitation::Path::getPath<SinglePath>(
      BuildRoot, DriverDefaults::HEADER_DIR_SUFFIX
  );
}

void LevitationDriver::setExtraPreambleArgs(StringRef Args) {
  ExtraPreambleArgs = ArgsUtils::parse(Args);
}

void LevitationDriver::setExtraParserArgs(StringRef Args) {
  ExtraParseArgs = ArgsUtils::parse(Args);
}

void LevitationDriver::setExtraCodeGenArgs(StringRef Args) {
  ExtraCodeGenArgs = ArgsUtils::parse(Args);
}

void LevitationDriver::setExtraLinkerArgs(StringRef Args) {
  ExtraLinkerArgs = ArgsUtils::parse(Args);
}

bool LevitationDriver::run() {

  log::Logger::createLogger(log::Level::Info);
  TasksManager::create(JobsNumber);
  CreatableSingleton<FileManager>::create( FileSystemOptions { StringRef() });
  CreatableSingleton<DependenciesStringsPool >::create();

  initParameters();

  RunContext Context(*this);
  LevitationDriverImpl Impl(Context);

  Impl.collectSources();
  Impl.buildPreamble();
  Impl.runParseImport();
  Impl.solveDependencies();
  Impl.codeGen();

  if (LinkPhaseEnabled)
    Impl.runLinker();

  if (Context.Status.hasWarnings()) {
    log::Logger::get().warning()
    << Context.Status.getWarningMessage();
  }

  if (!Context.Status.isValid()) {
    log::Logger::get().error()
    << Context.Status.getErrorMessage();
    return false;
  }

  return true;
}

void LevitationDriver::initParameters() {
  if (Output.empty()) {
    Output = isLinkPhaseEnabled() ?
        DriverDefaults::OUTPUT_EXECUTABLE :
        DriverDefaults::OUTPUT_OBJECTS_DIR;
  }

  if (Verbose) {
    log::Logger::get().setLogLevel(log::Level::Verbose);
    dumpParameters();
  }
}

void LevitationDriver::dumpParameters() {

  auto &Out = log::Logger::get().verbose();

  Out
  << "\n"
  << "  Running driver with following parameters:\n\n"
  << "    BinaryDir: " << BinDir << "\n"
  << "    SourcesRoot: " << SourcesRoot << "\n"
  << "    PreambleSource: " << (PreambleSource.empty() ? "<preamble compilation not requested>" : PreambleSource) << "\n"
  << "    JobsNumber: " << JobsNumber << "\n"
  << "    Output: " << Output << "\n"
  << "    OutputHeadersDir: " << (isLinkPhaseEnabled() ? "<n/a>" : OutputHeadersDir.c_str()) << "\n"
  << "    DryRun: " << (DryRun ? "yes" : "no") << "\n"
  << "\n";

  dumpExtraFlags("Preamble", ExtraPreambleArgs);
  dumpExtraFlags("Parse", ExtraParseArgs);
  dumpExtraFlags("CodeGen", ExtraCodeGenArgs);
  dumpExtraFlags("Link", ExtraLinkerArgs);

  Out << "\n";
}

void LevitationDriver::dumpExtraFlags(StringRef Phase, const Args &args) {

  if (args.empty())
    return;

  auto &Out = log::Logger::get().verbose();

  Out << "Extra args, phase '" << Phase << "':\n";

  Out << "  ";

  ArgsUtils::dump(Out, args);

  Out << "\n";
}

}}}
