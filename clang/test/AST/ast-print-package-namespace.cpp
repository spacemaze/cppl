// RUN: %clang_cc1 -cc1 -x c++ -flevitation-mode -flevitation-build-stage=ast -std=c++17 -ast-print %s -o - | FileCheck %s

// CHECK: package namespace A {
// CHECK-NEXT: package namespace B {
// CHECK-NEXT: }
// CHECK-NEXT: }
package namespace A::B {}