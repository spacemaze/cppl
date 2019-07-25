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

    std::shared_ptr<SolvedDependenciesInfo> DependenciesInfo;

    RunContext(LevitationDriver &driver) : Driver(driver) {}
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
  );

  static bool instantiateDecl(
      StringRef OutDeclASTFile,
      StringRef ASTFile,
      Paths Deps
  );

  static bool instantiateObject(
      StringRef OutObjFile,
      StringRef ASTFile,
      Paths Deps
  );

  static bool link(StringRef OutputFile, const Paths &ObjectFiles);
};

void LevitationDriverImpl::runParse() {
  auto &TM = TasksManager::get();

  for (auto PackagePath : Context.Packages) {

    auto Files = Context.Files[PackagePath];

    TM.addTask([=] (TaskContext &TC) {
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
  Solver.setMainFile(Context.Driver.MainSource);
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

  auto &DependenciesInfo = *Context.DependenciesInfo;

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

  auto &TM = TasksManager::get();

  Paths ObjectFiles;
  for (auto &PackagePath : Context.Packages) {
    assert(Context.Files.count(PackagePath));
    ObjectFiles.push_back(Context.Files[PackagePath].LDeps);
  }

  auto Res = TM.executeTask(
      [&] (TaskContext &TC) {
        TC.Successful = Commands::link(
            Context.Driver.Output, ObjectFiles
        );
      }
  );

  if (!Res)
    Status.setFailure()
    << "Link: phase failed";
}

void LevitationDriverImpl::collectSources() {

  Log.verbose() << "Collecting sources...\n";

  FileSystem::collectFiles(
          Context.Packages,
          Context.Driver.SourcesRoot,
          FileExtensions::SourceCode
  );

  for (const auto &Src : Context.Packages) {
    SinglePath PackagePath = Path::makeRelative<SinglePath>(
        Src, Context.Driver.SourcesRoot
    );

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

    auto Res = Context.Files.insert({ Src, Files });

    assert(Res.second);
  }

  Log.verbose()
  << "Found " << Context.Packages.size()
  << " '." << FileExtensions::SourceCode << "' files.\n\n";
}

bool LevitationDriverImpl::processDependencyNode(
    const DependenciesGraph::Node &N
) {
  const auto &Strings = Context.DependenciesInfo->getStrings();
  const auto &Graph = Context.DependenciesInfo->getDependenciesGraph();

  const auto &SrcRel = *Strings.getItem(N.PackageInfo->PackagePath);
  const auto &Files = Context.Files[SrcRel];

  auto &fullDependencieIDs = Context.DependenciesInfo->getDependenciesList(N.ID);

  Paths fullDependencies;
  for (auto DID : fullDependencieIDs) {
    auto &DNode = Graph.getNode(DID.NodeID);
    auto DepPath = *Strings.getItem(DNode.PackageInfo->PackagePath);
    fullDependencies.push_back(DepPath);
  }

  switch (N.Kind) {

    case DependenciesGraph::NodeKind::Declaration:
      return Commands::instantiateDecl(
          Files.DeclAST, Files.AST, fullDependencies
      );

    case DependenciesGraph::NodeKind::Definition:
      return Commands::instantiateObject(
          Files.Object, Files.AST, fullDependencies
      );

    default:
      llvm_unreachable("Unknown dependency kind");
  }
}

//-----------------------------------------------------------------------------
//  LevitationDriver

LevitationDriver::LevitationDriver()
{}

bool LevitationDriver::run() {

  log::Logger::createLogger();
  TasksManager::create(JobsNumber);

  initParameters();

  RunContext Context(*this);
  LevitationDriverImpl Impl(Context);

  Impl.collectSources();
  Impl.runParse();
  Impl.solveDependencies();
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
