// RUN: %clang_cc1 -xc++ -std=c++17 -levitation-build-ast %s -o %t.ast
// RUN: %clang_cc1 -std=c++17 -flevitation-build-object %t.ast -ast-print | FileCheck %s

// CHECK: package namespace A {
// CHECK-NEXT: package namespace B {
// CHECK-NEXT: }
// CHECK-NEXT: }
package namespace A::B {}