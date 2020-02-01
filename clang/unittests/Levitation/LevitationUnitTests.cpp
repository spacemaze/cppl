//===--- C++ Levitation LevitationUnitTests.cpp -----------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation Unit Tests
//
//===----------------------------------------------------------------------===//

#include "clang/Levitation/Common/SimpleLogger.h"
#include "clang/Levitation/TasksManager/TasksManager.h"
#include "llvm/Support/raw_ostream.h"
#include "gtest/gtest.h"

#include <chrono>

using namespace llvm;
using namespace clang;
using namespace levitation;

namespace {
class LevitationUnitTests : public ::testing::Test {
protected:
  void SetUp() override {
    log::Logger::createLogger(log::Level::Verbose);
  }
};

TEST_F(LevitationUnitTests, FirstTest) {

  bool Flag = false;

  {
    tasks::TasksManager TM(1);

    TM.addTask([&](tasks::TasksManager::TaskContext &Context) {
      std::this_thread::sleep_for(std::chrono::seconds(5));
      Flag = true;
      Context.Successful = true;
    });

    TM.waitForTasks();
  }

  EXPECT_TRUE(Flag);
}
}
