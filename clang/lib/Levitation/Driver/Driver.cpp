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
#include "clang/Levitation/TasksManager/Task.h"
#include "clang/Levitation/TasksManager/TasksManager.h"

#include <memory>
#include <utility>

namespace clang { namespace levitation { namespace tools {

using namespace clang::levitation::dependencies_solver;

//-----------------------------------------------------------------------------
//  Levitation driver implementation classes
//  (LevitationDriver itself is defined at the bottom)

namespace {

  struct RunContext {
    LevitationDriver &Driver;
    Failable Status;
    Paths Sources;
    Paths LDepsFiles;
    Paths ObjectFiles;
    std::shared_ptr<SolvedDependenciesInfo> DependenciesInfo;

    RunContext(LevitationDriver &driver) : Driver(driver) {}
  };
}

class LevitationDriverImpl {
  RunContext &Context;

  Failable &Status;
  log::Logger &Log;
  tasks::TasksManager &TM;

public:

  explicit LevitationDriverImpl(RunContext &context)
  : Context(context),
    Status(context.Status),
    Log(log::Logger::get()),
    TM(tasks::TasksManager::get())
  {}

  void runParse();
  void solveDependencies();
  void instantiateAndCodeGen();
  void runLinker();

  void collectSources();
  tasks::Task createParseTask(llvm::StringRef Source);
  tasks::Task createInstantiateTask(llvm::StringRef Source);
  tasks::Task createCodeGenTask(llvm::StringRef Source);
  tasks::Task createParseAndEmitTask(llvm::StringRef Source);
  tasks::Task createLinkTask(const Paths &ObjectFiles);

  void processDependencyNode(const DependenciesGraph::Node &N);
};

void LevitationDriverImpl::runParse() {
  auto &TM = tasks::TasksManager::get();

  for (auto Source : Context.Sources) {
    auto parseTask = createParseTask(Source);
    TM.addTask(std::move(parseTask));
  }

  TM.waitForTasks();

  Status.inheritResult(TM, "Parse: ");
}

void LevitationDriverImpl::solveDependencies() {
  if (!Status.isValid())
    return;

  DependenciesSolver Solver;
  Solver.setSourcesRoot(Context.Driver.SourcesRoot);
  Solver.setBuildRoot(Context.Driver.BuildRoot);
  Solver.setMainFile(Context.Driver.MainSource);
  Solver.setVerbose(Context.Driver.Verbose);

  Context.DependenciesInfo = Solver.solve(Context.LDepsFiles);

  Status.inheritResult(Solver, "Dependencies solver: ");
}

void LevitationDriverImpl::instantiateAndCodeGen() {
  if (!Status.isValid())
    return;

  auto &DependenciesInfo = *Context.DependenciesInfo;

  Context.DepsGraph.dsfJobs(
      Context.Driver.JobsNumber,
      Context.Status,
      [=] (const DependenciesGraph::Node &N) {
        processDependencyNode(N);
      }
  );

  Status.inheritResult(Solver, "Instantiate and codegen: ");
}

void LevitationDriverImpl::runLinker() {
  if (!Status.isValid())
    return;

  assert(Context.Driver.isLinkPhaseEnabled() && "Link phase must be enabled.");

  auto &TM = tasks::TasksManager::get();

  auto linkTask = createLinkTask(Context.ObjectFiles);
  TM.executeTask(std::move(linkTask));

  Status.inheritResult(TM, "Parse: ");
}

void LevitationDriverImpl::collectSources() {

  Log.verbose() << "Collecting sources...\n";

  FileSystem::collectFiles(
      Context.Sources,
      Context.Driver.SourcesRoot,
      FileExtensions::SourceCode
  );

  Path::replaceExtension(
      Context.LDepsFiles,
      Context.Sources,
      FileExtensions::ParsedDependencies
  );

  Path::replaceExtension(
      Context.ObjectFiles,
      Context.Sources,
      FileExtensions::Object
  );

  Log.verbose()
  << "Found " << Context.Sources.size()
  << " '." << FileExtensions::SourceCode << "' files.\n\n";
}

void LevitationDriverImpl::processDependencyNode(
    const DependenciesGraph::Node &N
) {

  auto &fullDependencies = Context.DependenciesInfo->getDependenciesList(N.ID);

  switch (N.Kind) {

    case DependenciesGraph::NodeKind::Declaration: {
        // Launch instantiation of .decl-ast file.
        // TODO Levitation: get .ast file for given dependency node
        auto &astFile = Context.SolvedDependencies.getASTFile(N.ID);
        runDeclInstantiation(astFile, fullDependencies);
      }
      break;

    case DependenciesGraph::NodeKind::Definition: {
        // Launch instantiation of .o file.
        // TODO Levitation: get .o file for given dependency node
        auto &astFile = Context.SolvedDependencies.getASTFile(N.ID);
        runObjectInstantiation(astFile, fullDependencies);
      }
      break;

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
  tasks::TasksManager::create(JobsNumber);

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
