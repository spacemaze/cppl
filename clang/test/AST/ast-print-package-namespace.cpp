// RUN: %clang_cc1 -xc++ -std=c++17 -levitation-build-ast -levitation-deps-output-file=/dev/null -levitation-sources-root-dir=. %s -o %t.ast
// RUN: %clang_cc1 -std=c++17 -flevitation-build-object %t.ast -ast-print | FileCheck %s

// CHECK: package namespace A {
// CHECK-NEXT: package namespace B {
// CHECK-NEXT: }
// CHECK-NEXT: }
package namespace A::B {}