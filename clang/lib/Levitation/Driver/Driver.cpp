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
#include "clang/Levitation/Common/FileSystem.h"
#include "clang/Levitation/Common/SimpleLogger.h"
#include "clang/Levitation/DependenciesSolver/DependenciesGraph.h"
#include "clang/Levitation/DependenciesSolver/DependenciesSolverPath.h"
#include "clang/Levitation/DependenciesSolver/DependenciesSolver.h"
#include "clang/Levitation/DependenciesSolver/SolvedDependenciesInfo.h"
#include "clang/Levitation/Driver/Driver.h"
#include "clang/Levitation/FileExtensions.h"
#include "clang/Levitation/TasksManager/TasksManager.h"

#include "llvm/Support/Program.h"
#include "llvm/ADT/None.h"

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
    SinglePath LDeps;
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
    llvm::DenseMap<StringRef, FilesInfo> Files;
    SinglePath MainFileNormilized;

    std::shared_ptr<SolvedDependenciesInfo> DependenciesInfo;

    RunContext(LevitationDriver &driver)
    : Driver(driver),
      MainFileNormilized(levitation::Path::makeRelative<SinglePath>(
          driver.getMainSource(), driver.getSourcesRoot()
      ))
    {}
  };
}

class LevitationDriverImpl {
  RunContext &Context;

  Failable &Status;
  log::Logger &Log;
  TasksManager &TM;

public:

  explicit LevitationDriverImpl(RunContext &context)
  : Context(context),
    Status(context.Status),
    Log(log::Logger::get()),
    TM(TasksManager::get())
  {}

  void buildPreamble();
  void runParse();
  void solveDependencies();
  void instantiateAndCodeGen();
  void runLinker();

  void collectSources();

  void addMainFileInfo();

  bool processDependencyNode(
      const DependenciesGraph::Node &N
  );
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
      ++ArgStart;
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
        Args.emplace_back(ArgsString.substr(ArgStart, ArgEnd - ArgStart));
        return;
      }

      Twine Arg(ArgsString.substr(ArgStart, ArgEscapes[0] - ArgStart));

      for (size_t i = 1, e = ArgEscapes.size(); i != e; ++i) {
        size_t Start = ArgEscapes[i-1] + 1;
        size_t End = ArgEscapes[i];
        Arg.concat(ArgsString.substr(Start, End - Start));
      }

      Args.push_back(Arg.str());
    }

  public:

    ArgsBuilder(StringRef argsString)
    : ArgsString(argsString)
    {}

    ~ArgsBuilder() {}

    void onQuote(QuoteType quoteType) {
      if (!isQuoteOpened()) {
        QuoteOpened = quoteType;

        if (!isCurArgEmpty()) {
          onRegularSymbol();
          return;
        }

        newStartPos();

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
    }

    void detachArgsTo(LevitationDriver::Args &Dest) {
      commitArg();
      Dest.swap(Args);
    }
  };
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
      Args.push_back(A);
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
        bool verbose,
        bool dryRun
    ) {
      auto Cmd = getBase(
          BinDir,
          /*PrecompiledPreamble=*/ "",
          verbose,
          dryRun
      );

      Cmd
      .addArg("-xc++")
      .addArg("-levitation-build-preamble");

      return Cmd;
    }

    static CommandInfo getParse(
        StringRef BinDir,
        StringRef PrecompiledPreamble,
        bool verbose,
        bool dryRun
    ) {
      auto Cmd = getBase(BinDir, PrecompiledPreamble, verbose, dryRun);
      Cmd
      .addArg("-levitation-build-ast")
      .addArg("-xc++");

      return Cmd;
    }
    static CommandInfo getInstDecl(
        StringRef BinDir,
        StringRef PrecompiledPreamble,
        bool verbose,
        bool dryRun
    ) {
      auto Cmd = getBase(BinDir, PrecompiledPreamble, verbose, dryRun);
      Cmd
      .addArg("-flevitation-build-decl")
      .addArg("-emit-pch");
      return Cmd;
    }

    static CommandInfo getInstObj(
        StringRef BinDir,
        StringRef PrecompiledPreamble,
        bool verbose,
        bool dryRun
    ) {
      auto Cmd = getBase(BinDir, PrecompiledPreamble, verbose, dryRun);
      Cmd
      .addArg("-flevitation-build-object")
      .addArg("-emit-obj");
      return Cmd;
    }
    static CommandInfo getCompileSrc(
        StringRef BinDir,
        StringRef PrecompiledPreamble,
        bool verbose,
        bool dryRun
    ) {
      auto Cmd = getBase(BinDir, PrecompiledPreamble, verbose, dryRun);
      Cmd
      .addArg("-flevitation-build-object")
      .addArg("-xc++")
      .addArg("-emit-obj");
      return Cmd;
    }
    static CommandInfo getLink(
        StringRef BinDir,
        bool verbose,
        bool dryRun
    ) {
      CommandInfo Cmd(getClangXXPath(BinDir), verbose, dryRun);
      Cmd.addArg("-stdlib=libstdc++");
      return Cmd;
    }

    CommandInfo& addArg(StringRef Arg) {
      CommandArgs.emplace_back(Arg);
      return *this;
    }

    CommandInfo& addKVArgSpace(StringRef Arg, StringRef Value) {
      CommandArgs.emplace_back(Arg);
      CommandArgs.emplace_back(Value);
      return *this;
    }

    CommandInfo& addKVArgEq(StringRef Arg, StringRef Value) {
      OwnArgs.emplace_back((Arg + "=" + Value).str());
      CommandArgs.emplace_back(OwnArgs.back());
      return *this;
    }

    template <typename ValuesT>
    CommandInfo& addArgs(const ValuesT& Values) {
      for (const auto &Value : Values) {
        CommandArgs.emplace_back(Value);
      }
      return *this;
    }

    template <typename ValuesT>
    CommandInfo& addKVArgsEq(StringRef Name, const ValuesT Values) {
      for (const auto &Value : Values) {
        OwnArgs.emplace_back((Name + "=" + Value).str());
        CommandArgs.emplace_back(OwnArgs.back());
      }
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

    template <typename ValueT>
    void addOwnArg(const ValueT& V) {
      OwnArgs.emplace_back(V);
    }

    void addOwnArg(const Twine& V) {
      OwnArgs.emplace_back(V.str());
    }

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
    .addKVArgEq(
        "-levitation-sources-root-dir",
        SourcesRoot
    )
    .addKVArgEq(
        "-levitation-deps-output-file",
        OutLDepsFile
    )
    .addArg(SourceFile)
    .addKVArgSpace("-o", OutASTFile)
    .execute();

    return processStatus(ExecutionStatus);
  }

  static bool instantiateDecl(
      StringRef BinDir,
      StringRef PrecompiledPreamble,
      StringRef OutDeclASTFile,
      StringRef InputObject,
      const Paths &Deps,
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
    .addKVArgsEq("-levitation-dependency", Deps)
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
    .addKVArgsEq("-levitation-dependency", Deps)
    .addArg(InputObject)
    .addKVArgSpace("-o", OutObjFile)
    .execute();

    return processStatus(ExecutionStatus);
  }

  static bool buildPreamble(
      StringRef BinDir,
      StringRef PreambleSource,
      StringRef PCHOutput,
      const LevitationDriver::Args &ExtraPreambleArgs,
      bool Verbose,
      bool DryRun
  ) {
    assert(PreambleSource.size() && PCHOutput.size());

    if (!DryRun || Verbose)
      dumpBuildPreamble(PreambleSource, PCHOutput);

    levitation::Path::createDirsForFile(PCHOutput);

    auto ExecutionStatus = CommandInfo::getBuildPreamble(
        BinDir, Verbose, DryRun
    )
    .addArg(PreambleSource)
    .addKVArgSpace("-o", PCHOutput)
    .addArgs(ExtraPreambleArgs)
    .execute();

    return processStatus(ExecutionStatus);
  }

  static bool compileMain(
      StringRef BinDir,
      StringRef PrecompiledPreamble,
      StringRef OutObjFile,
      StringRef InputObject,
      const Paths &Deps,
      const LevitationDriver::Args &ExtraParseArgs,
      const LevitationDriver::Args &ExtraCodeGenArgs,
      bool Verbose,
      bool DryRun
  ) {
    assert(OutObjFile.size() && InputObject.size());

    if (!DryRun || Verbose)
      dumpCompileMain(OutObjFile, InputObject, Deps);

    levitation::Path::createDirsForFile(OutObjFile);

    auto ExecutionStatus = CommandInfo::getCompileSrc(
        BinDir, PrecompiledPreamble, Verbose, DryRun
    )
    .addKVArgsEq("-levitation-dependency", Deps)
    .addArg(InputObject)
    .addKVArgSpace("-o", OutObjFile)
    .execute();

    return processStatus(ExecutionStatus);
  }

  static bool link(
      StringRef BinDir,
      StringRef OutputFile,
      const Paths &ObjectFiles,
      const LevitationDriver::Args &ExtraArgs,
      bool Verbose,
      bool DryRun
  ) {
    assert(OutputFile.size() && ObjectFiles.size());

    if (!DryRun || Verbose)
      dumpLink(OutputFile, ObjectFiles);

    levitation::Path::createDirsForFile(OutputFile);

    auto ExecutionStatus = CommandInfo::getLink(
        BinDir, Verbose, DryRun
    )
    .addArgs(ObjectFiles)
    .addKVArgSpace("-o", OutputFile)
    .execute();

    return true;
  }

protected:

  static void dumpBuildPreamble(
      StringRef PreambleSource,
      StringRef PreambleOut
  ) {
    auto &LogInfo = log::Logger::get().info();
    LogInfo
    << "PREAMBLE " << PreambleSource << " -> "
    << "preamble out: " << PreambleOut << "\n";
  }

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

  static void dumpInstantiateDecl(
      StringRef OutDeclASTFile,
      StringRef InputObject,
      const Paths &Deps
  ) {
    assert(OutDeclASTFile.size() && InputObject.size());

    dumpInstantiate(OutDeclASTFile, InputObject, Deps, "INST DECL", "decl-ast");
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

    auto &LogInfo = log::Logger::get().info();
    LogInfo << ActionName << " " << InputObject;

    LogInfo << ", ";
    dumpLDepsFiles(LogInfo, Deps);

    LogInfo << " -> " << OutputName << ": " << OutDeclASTFile << "\n";
  }

  static void dumpLDepsFiles(
      raw_ostream &Out,
      const Paths &Deps
  ) {
    dumpPathsArray(Out, Deps, "deps");
  }

  static void dumpLink(StringRef OutputFile, const Paths &ObjectFiles) {
    assert(OutputFile.size() && ObjectFiles.size());

    auto &LogInfo = log::Logger::get().info();

    LogInfo << "LINK ";

    dumpObjectFiles(LogInfo, ObjectFiles);

    LogInfo << " -> " << OutputFile << "\n";
  }

  static void dumpObjectFiles(
      raw_ostream &Out,
      const Paths &ObjectFiles
  ) {
    dumpPathsArray(Out, ObjectFiles, "objects");
  }

  static void dumpPathsArray(
      raw_ostream &Out,
      const Paths &ObjectFiles,
      StringRef ArrayName
  ) {
    Out << ArrayName << ": ";

    if (ObjectFiles.size()) {
      Out << "(";
      for (size_t i = 0, e = ObjectFiles.size(); i != e; ++i) {
        log::Logger::get().info() << ObjectFiles[i];
        if (i + 1 != e)
          log::Logger::get().info() << ", ";
      }
      Out << ")";
    } else {
      Out << "<empty>";
    }
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
  }

  auto Res = Commands::buildPreamble(
    Context.Driver.BinDir,
    Context.Driver.PreambleSource,
    Context.Driver.PreambleOutput,
    Context.Driver.ExtraPreambleArgs,
    Context.Driver.Verbose,
    Context.Driver.DryRun
  );

  if (!Res)
    Status.setFailure()
    << "Preamble: phase failed";
}

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

void LevitationDriverImpl::solveDependencies() {
  if (!Status.isValid())
    return;

  DependenciesSolver Solver;
  Solver.setSourcesRoot(Context.Driver.SourcesRoot);
  Solver.setBuildRoot(Context.Driver.BuildRoot);
  Solver.setMainFile(Context.MainFileNormilized);
  Solver.setVerbose(Context.Driver.Verbose);

  Paths LDepsFiles;
  for (auto &PackagePath : Context.Packages) {
    assert(Context.Files.count(PackagePath));
    LDepsFiles.push_back(Context.Files[PackagePath].LDeps);
  }

  Context.DependenciesInfo = Solver.solve(LDepsFiles);

  Status.inheritResult(Solver, "Dependencies solver: ");
}

void LevitationDriverImpl::instantiateAndCodeGen() {
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
      Context.Driver.ExtraLinkerArgs,
      Context.Driver.Verbose,
      Context.Driver.DryRun
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

    Files.Source = PackagePath;
    //  Files.Source = Path::getPath<SinglePath>(
    //      Context.Driver.SourcesRoot,
    //      PackagePath,
    //      FileExtensions::SourceCode
    //  );

    Files.LDeps = Path::getPath<SinglePath>(
        Context.Driver.BuildRoot,
        PackagePath,
        FileExtensions::ParsedDependencies
    );
    Files.AST = Path::getPath<SinglePath>(
        Context.Driver.BuildRoot,
        PackagePath,
        FileExtensions::ParsedAST
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

void LevitationDriverImpl::addMainFileInfo() {
  if (!Context.Status.isValid())
    return;

  // Inject main file package
  Context.Packages.emplace_back(Context.MainFileNormilized);

  const auto &PackagePath = Context.Packages.back();

  FilesInfo Files;

  // In current implementation package path is equal to relative source path.

  Files.Source = PackagePath;
  //  Files.Source = Path::getPath<SinglePath>(
  //      Context.Driver.SourcesRoot,
  //      PackagePath,
  //      FileExtensions::SourceCode
  //  );

  Files.Object = Path::getPath<SinglePath>(
      Context.Driver.BuildRoot,
      PackagePath,
      FileExtensions::Object
  );

  Log.verbose() << "Injected main file '" << PackagePath << "'\n";

  auto Res = Context.Files.insert({ PackagePath, Files });

  assert(Res.second);
}

bool LevitationDriverImpl::processDependencyNode(
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

  auto &fullDependencieIDs = Context.DependenciesInfo->getDependenciesList(N.ID);

  Paths fullDependencies;
  for (auto DID : fullDependencieIDs) {
    auto &DNode = Graph.getNode(DID.NodeID);
    auto DepPath = *Strings.getItem(DNode.PackageInfo->PackagePath);

    DependenciesSolverPath::addDepPathsFor(
        fullDependencies,
        Context.Driver.BuildRoot,
        DepPath
    );

    assert(
        DepPath != Context.MainFileNormilized &&
        "Main file can't be a dependency"
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
          Context.Driver.Verbose,
          Context.Driver.DryRun
      );

    case DependenciesGraph::NodeKind::Definition: {
      if (N.PackageInfo->IsMainFile) {
        return Commands::compileMain(
          Context.Driver.BinDir,
          Context.Driver.PreambleOutput,
          Files.Object,
          Files.Source,
          fullDependencies,
          Context.Driver.ExtraParseArgs,
          Context.Driver.ExtraCodeGenArgs,
          Context.Driver.Verbose,
          Context.Driver.DryRun
        );
      } else {
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
  Impl.runParse();
  Impl.solveDependencies();
  Impl.addMainFileInfo();
  Impl.instantiateAndCodeGen();

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
  << "    MainSource: " << MainSource << "\n"
  << "    PreambleSource: " << (PreambleSource.empty() ? "<preamble compilation not requested>" : PreambleSource) << "\n"
  << "    JobsNumber: " << JobsNumber << "\n"
  << "    Output: " << Output << "\n"
  << "    OutputHeader: " << (OutputHeader.empty() ? "<header creation not requested>" : OutputHeader) << "\n"
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

  Out
  << "Extra args, phase '" << Phase << "':\n"
  << "  \"";

  ArgsUtils::dump(Out, args);

  Out
  << "\"\n";
}

}}}
