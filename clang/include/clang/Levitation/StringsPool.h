//
// Created by Stepan Dyatkovskiy on 6/7/19.
//

#ifndef LLVM_CLANG_LEVITATION_STRINGSPOOL_H
#define LLVM_CLANG_LEVITATION_STRINGSPOOL_H

#include "clang/Levitation/IndexedSet.h"
#include "llvm/ADT/SmallString.h"

namespace clang { namespace levitation {

using StringID = uint64_t;

template<unsigned N>
using StringsPool = IndexedSet<StringID, llvm::SmallString<N> >;

}}

#endif //LLVM_STRINGSPOOL_H
