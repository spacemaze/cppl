// TODO Levitation: Licensing

// This header adds support for python-like "with" operator.

#ifndef LLVM_CLANG_LEVITATION_WITHOPERATOR_H
#define LLVM_CLANG_LEVITATION_WITHOPERATOR_H

#include "llvm/ADT/ScopeExit.h"

namespace clang {
namespace levitation {

  #define with if

  class WithOperand {
  public:
    operator bool() const { return true; }
  };

  template <typename Callable>
  class ScopeExit : public WithOperand {
    llvm::detail::scope_exit<typename std::decay<Callable>::type> Scope;
  public:
    ScopeExit(Callable &&ExitF) : Scope(llvm::make_scope_exit(std::move(ExitF))) {}
  };

  template <typename Callable>
  ScopeExit<Callable> make_scope_exit(Callable &&ExitF) {
    return ScopeExit<Callable>(std::move(ExitF));
  }

}
}

#endif //LLVM_WITHOPERATOR_H
