// TODO Levitation: Licensing

#ifndef LLVM_CLANG_LEVITATION_DEPENDENCIESSEMASOURCE_H
#define LLVM_CLANG_LEVITATION_DEPENDENCIESSEMASOURCE_H

#include "clang/Sema/MultiplexExternalSemaSource.h"

#include <memory>

namespace clang {

class ASTReader;

namespace levitation {

// So far we have nothing to override in MultiplexExternalSemaSource.
class DependenciesSemaSource : public MultiplexExternalSemaSource {
  SmallVector<std::unique_ptr<ASTReader>, 32> DepReaders;
public:
  void addSource(std::unique_ptr<ASTReader> &&StandaloneReader) {
    MultiplexExternalSemaSource::addSource(*StandaloneReader);
    DepReaders.push_back(std::move(StandaloneReader));
  }
};

}
}

#endif //LLVM_CLANG_LEVITATION_DEPENDENCIESSEMASOURCE_H
