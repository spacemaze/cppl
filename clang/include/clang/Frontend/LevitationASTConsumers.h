//===-- LevitationASTConsumers.h - Levitation AST Consumers -*- C++ -----*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_FRONTEND_LEVITATIONASTCONSUMERS_H
#define LLVM_CLANG_FRONTEND_LEVITATIONASTCONSUMERS_H

#include "clang/Basic/LLVM.h"
#include <memory>
#include <vector>

namespace clang {

class ASTConsumer;
class CompilerInstance;
class Preprocessor;
struct PCHBuffer;

namespace levitation {

  class LevitationPreprocessorConsumer {
  public:
    virtual void HandlePreprocessor(Preprocessor &PP) = 0;
    virtual ~LevitationPreprocessorConsumer() = default;
  };

  class LevitationMultiplexPreprocessorConsumer : public LevitationPreprocessorConsumer{
    std::vector<std::unique_ptr<LevitationPreprocessorConsumer>> Consumers;
  public:
    LevitationMultiplexPreprocessorConsumer(
        std::vector<std::unique_ptr<LevitationPreprocessorConsumer>> &&consumers
    ) :
      Consumers(std::move(consumers))
    {}

    void HandlePreprocessor(Preprocessor &PP) override;
  };

std::unique_ptr<LevitationPreprocessorConsumer> CreateDependenciesASTProcessor(
    CompilerInstance &CI
);

std::unique_ptr<ASTConsumer> CreateUnitNamespaceVerifier(
    CompilerInstance &CI
);

} // end of namespace levitation
} // end of namespace clang

#endif
