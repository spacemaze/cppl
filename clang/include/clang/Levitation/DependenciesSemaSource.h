// TODO Levitation: Licensing

#ifndef LLVM_CLANG_LEVITATION_DEPENDENCIESSEMASOURCE_H
#define LLVM_CLANG_LEVITATION_DEPENDENCIESSEMASOURCE_H

#include "clang/Sema/MultiplexExternalSemaSource.h"

namespace clang {
namespace levitation {

// So far we have nothing to override in MultiplexExternalSemaSource.
using DependenciesSemaSource = clang::MultiplexExternalSemaSource;

}
}

#endif //LLVM_CLANG_LEVITATION_DEPENDENCIESSEMASOURCE_H
