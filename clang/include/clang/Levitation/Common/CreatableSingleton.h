//===--- C++ Levitation CreatableSingleton.h ------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation CreatableSingleton template.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_CREATABLESINGLETON_H
#define LLVM_CLANG_LEVITATION_CREATABLESINGLETON_H

#include <memory>
#include <utility>

namespace clang { namespace levitation {

/// CreatableSingleton is a Singleton pattern, which also allows to control
/// singleton lifetime through its creation method.
/// It is also possible to introduce a destruction as well,
/// but there is no need so far.
///
/// example of use:
///
/// class MyClass : public CreatableSingleton<MyClass> {
/// protected:
///   MyClass(int p0, float p1) {}
///   friend CreatableSingleton<MyClass>;
/// public:
///   // Define public interface here
///   void use() {}
/// };
///
/// void init() {
///   MyClass::create(1, 2.0);
/// }
///
/// void usage() {
///   MyClass::get().use();
/// }
///
/// \tparam DerivedTy class you want to make a singleton.
///
template<class DerivedTy>
class CreatableSingleton {
protected:

  static std::unique_ptr<DerivedTy> &accessPtr() {
    static std::unique_ptr<DerivedTy> Ptr;
    return Ptr;
  }

public:

  template<typename ...ArgTys>
  static DerivedTy &create(ArgTys &&...Args) {
    accessPtr() =
    std::unique_ptr<DerivedTy>(new DerivedTy(std::forward<ArgTys(Args)...));

    return get();
  }

  static DerivedTy &get() {
    auto &Ptr = accessPtr();
    assert(Ptr && "Instance should be created");
    return *Ptr;
  }
};

}}

#endif //LLVM_CLANG_LEVITATION_CREATABLESINGLETON_H
