// This is a generated file. Don't edit it.
// Edit main.cpp.in and use bash.sh or test-all.sh
// to generate it again.
// ------------------------------------------------

// RUN:  %clang -cc1 -std=c++17 -xc++ -levitation-build-preamble %S/../preamble.hpp -o %T/preamble.pch
// Parsing 'P1/A'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -levitation-build-ast -levitation-sources-root-dir=%S -levitation-deps-output-file=%T/P1_A.ldeps %S/P1/A.cppl -o %T/P1_A.ast
// Parsing 'P2/B'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -levitation-build-ast -levitation-sources-root-dir=%S -levitation-deps-output-file=%T/P2_B.ldeps %S/P2/B.cppl -o %T/P2_B.ast
// Parsing 'P2/C'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -levitation-build-ast -levitation-sources-root-dir=%S -levitation-deps-output-file=%T/P2_C.ldeps %S/P2/C.cppl -o %T/P2_C.ast
// RUN:  levitation-deps -src-root=%S -build-root=%T -main-file=%S/main.cpp --verbose
// Instantiating 'P1/A'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-decl -emit-pch %T/P1_A.ast -o %T/P1_A.decl-ast
// Instantiating 'P2/C'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-decl -emit-pch %T/P2_C.ast -o %T/P2_C.decl-ast
// Instantiating 'P2/B'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-decl -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast -levitation-dependency=%T/P2_C.ast -levitation-dependency=%T/P2_C.decl-ast -emit-pch %T/P2_B.ast -o %T/P2_B.decl-ast
// Compiling 'P1/A'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-object -emit-obj %T/P1_A.ast -o %T/P1_A.o
// Compiling 'P2/C'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-object -emit-obj %T/P2_C.ast -o %T/P2_C.o
// Compiling 'P2/B'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-object -emit-obj -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast -levitation-dependency=%T/P2_C.ast -levitation-dependency=%T/P2_C.decl-ast %T/P2_B.ast -o %T/P2_B.o
// Compiling source 'main.cpp'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -flevitation-build-object -emit-obj -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast -levitation-dependency=%T/P2_C.ast -levitation-dependency=%T/P2_C.decl-ast -levitation-dependency=%T/P2_B.ast -levitation-dependency=%T/P2_B.decl-ast %S/main.cpp -o %T/main.o
// RUN:  %clangxx %T/main.o %T/P1_A.o %T/P2_C.o %T/P2_B.o -o %T/app.out
// RUN:  %T/app.out
int main() {
  with (
    auto TestScope = levitation::Test::context()
        .expect("P2::B::f()")
        .expect("P1::A::f()")
        .expect("P2::C::f()")
    .open()
  ) {
    P2::B::f();
  }
  return levitation::Test::result();
}
