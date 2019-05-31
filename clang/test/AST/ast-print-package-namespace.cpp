// RUN: %clang_cc1 -cc1 -x c++ -levitation-build-ast -levitation-deps-output-file=/dev/null -levitation-sources-root-dir=. -std=c++17 -ast-print %s -o - | FileCheck %s

// CHECK: package namespace A {
// CHECK-NEXT: package namespace B {
// CHECK-NEXT: }
// CHECK-NEXT: }
package namespace A::B {}