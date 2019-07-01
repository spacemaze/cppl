// This is a generated file. Don't edit it.
// Edit main.cpp.in and use bash.sh or test-all.sh
// to generate it again.
// ------------------------------------------------

// RUN:  %clang -cc1 -std=c++17 -xc++ -levitation-build-preamble %S/../preamble.hpp -o %T/preamble.pch
// Parsing 'P1/A'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -levitation-build-ast -levitation-sources-root-dir=%S -levitation-deps-output-file=%T/P1_A.ldeps %S/P1/A.cppl -o %T/P1_A.ast
// Instantiating 'P1/A'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-decl -emit-pch %T/P1_A.ast -o %T/P1_A.decl-ast
// Compiling 'P1/A'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-object -emit-obj %T/P1_A.ast -o %T/P1_A.o
// Compiling source 'main.cpp'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -flevitation-build-object -emit-obj -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast %S/main.cpp -o %T/main.o
// RUN:  %clangxx %T/main.o %T/P1_A.o -o %T/app.out
// RUN:  %T/app.out
int main() {
// CHECK: P1::A::AA::f()
  P1::A::AA::f();
  return 0;
}
