// RUN: %clang_cc1 -cc1 -x c++ -flevitation-mode -std=c++17 -triple x86_64-linux-gnu -ast-dump -ast-dump-filter Test %s | FileCheck -strict-whitespace %s

// CHECK: NamespaceDecl{{.*}} TestA
// CHECK-NEXT: `-NamespaceDecl{{.*}} TestB
package namespace TestA::TestB {}
