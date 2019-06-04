// RUN: %clang_cc1 -xc++ -std=c++17 -levitation-build-ast -levitation-deps-output-file=/dev/null -levitation-sources-root-dir=. %s -o %t.ast
// RUN: %clang_cc1 -std=c++17 -flevitation-build-object %t.ast -ast-dump -ast-dump-filter Test | FileCheck -strict-whitespace %s

// CHECK: NamespaceDecl{{.*}} TestA
// CHECK-NEXT: `-NamespaceDecl{{.*}} TestB
package namespace TestA::TestB {}
