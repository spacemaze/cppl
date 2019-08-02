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

#include <memory>
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
class Commands {
public:
  static bool parse(
      StringRef OutASTFile,
      StringRef OutLDepsFile,
      StringRef SourceFile
  ) {
    dumpParse(OutASTFile, OutLDepsFile, SourceFile);

    // TODO Levitation execute real command:
    //  return llvm::sys::ExecuteAndWait(
    //      Executable, Args, Env, Redirects, /*secondsToWait*/ 0,
    //      /*memoryLimit*/ 0, ErrMsg, ExecutionFailed);

    return true;
  }

  static bool instantiateDecl(
      StringRef OutDeclASTFile,
      StringRef InputObject,
      const Paths &Deps
  ) {
    assert(OutDeclASTFile.size() && InputObject.size());

    dumpInstantiateDecl(OutDeclASTFile, InputObject, Deps);

    // TODO Levitation execute real command:
    //  return llvm::sys::ExecuteAndWait(
    //      Executable, Args, Env, Redirects, /*secondsToWait*/ 0,
    //      /*memoryLimit*/ 0, ErrMsg, ExecutionFailed);

    return true;
  }

  static bool instantiateObject(
      StringRef OutObjFile,
      StringRef InputObject,
      const Paths &Deps
  ) {
    assert(OutObjFile.size() && InputObject.size());

    dumpInstantiateObject(OutObjFile, InputObject, Deps);

    // TODO Levitation execute real command:
    //  return llvm::sys::ExecuteAndWait(
    //      Executable, Args, Env, Redirects, /*secondsToWait*/ 0,
    //      /*memoryLimit*/ 0, ErrMsg, ExecutionFailed);

    return true;
  }

  static bool link(StringRef OutputFile, const Paths &ObjectFiles) {
    assert(OutputFile.size() && ObjectFiles.size());

    dumpLink(OutputFile, ObjectFiles);

    return true;
  }

protected:

  // TODO Levitation: should we bother with some cool dumping?
  void dumpKV(llvm::raw_ostream &out, StringRef K, StringRef V) {
    out << K << ": "; // ...
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
};

void LevitationDriverImpl::runParse() {
  auto &TM = TasksManager::get();

  for (auto PackagePath : Context.Packages) {

    auto Files = Context.Files[PackagePath];

    TM.addTask([=] (TasksManager::TaskContext &TC) {
      TC.Successful = Commands::parse(Files.AST, Files.LDeps, Files.Source);
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
      Context.Driver.Output, ObjectFiles
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
          Files.DeclAST, Files.AST, fullDependencies
      );

    case DependenciesGraph::NodeKind::Definition: {
      StringRef InputFile = N.PackageInfo->IsMainFile ?
          Files.Source : Files.AST;

      return Commands::instantiateObject(
        Files.Object, InputFile, fullDependencies
      );
    }

    default:
      llvm_unreachable("Unknown dependency kind");
  }
}

//-----------------------------------------------------------------------------
//  LevitationDriver

LevitationDriver::LevitationDriver()
{}

bool LevitationDriver::run() {

  log::Logger::createLogger(log::Level::Info);
  TasksManager::create(JobsNumber);
  CreatableSingleton<FileManager>::create( FileSystemOptions { StringRef() });
  CreatableSingleton<DependenciesStringsPool >::create();

  initParameters();

  RunContext Context(*this);
  LevitationDriverImpl Impl(Context);

  Impl.collectSources();
  Impl.runParse();
  Impl.solveDependencies();
  Impl.addMainFileInfo();
  Impl.instantiateAndCodeGen();

  if (LinkPhaseEnabled)
    Impl.runLinker();

  return Context.Status.isValid();
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
  log::Logger::get().verbose()
  << "\n"
  << "  Running driver with following parameters:\n\n"
  << "    SourcesRoot: " << SourcesRoot << "\n"
  << "    MainSource: " << MainSource << "\n"
  << "    PreambleSource: " << (PreambleSource.empty() ? "<preamble compilation not requested>" : PreambleSource) << "\n"
  << "    JobsNumber: " << JobsNumber << "\n"
  << "    Output: " << Output << "\n"
  << "    OutputHeader: " << (OutputHeader.empty() ? "<header creation not requested>" : OutputHeader) << "\n"
  << "\n";
}

}}}
