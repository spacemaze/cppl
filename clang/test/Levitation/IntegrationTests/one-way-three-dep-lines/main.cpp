// This is a generated file. Don't edit it.
// Edit main.cpp.in and use bash.sh or test-all.sh
// to generate it again.
// ------------------------------------------------

// RUN:  %clang -cc1 -std=c++17 -xc++ -levitation-build-preamble %S/../preamble.hpp -o %T/preamble.pch
// Parsing 'P1/A'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -levitation-build-ast -levitation-sources-root-dir=%S -levitation-deps-output-file=%T/P1_A.ldeps %S/P1/A.cppl -o %T/P1_A.ast
// Parsing 'P1/B'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -levitation-build-ast -levitation-sources-root-dir=%S -levitation-deps-output-file=%T/P1_B.ldeps %S/P1/B.cppl -o %T/P1_B.ast
// Parsing 'P1/C'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -levitation-build-ast -levitation-sources-root-dir=%S -levitation-deps-output-file=%T/P1_C.ldeps %S/P1/C.cppl -o %T/P1_C.ast
// Parsing 'P1/E'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -levitation-build-ast -levitation-sources-root-dir=%S -levitation-deps-output-file=%T/P1_E.ldeps %S/P1/E.cppl -o %T/P1_E.ast
// Parsing 'P1/F'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -levitation-build-ast -levitation-sources-root-dir=%S -levitation-deps-output-file=%T/P1_F.ldeps %S/P1/F.cppl -o %T/P1_F.ast
// RUN:  levitation-deps -src-root=%S -build-root=%T -main-file=%S/main.cpp --verbose
// Instantiating 'P1/A'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-decl -emit-pch %T/P1_A.ast -o %T/P1_A.decl-ast
// Instantiating 'P1/C'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-decl -emit-pch %T/P1_C.ast -o %T/P1_C.decl-ast
// Instantiating 'P1/B'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-decl -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast -emit-pch %T/P1_B.ast -o %T/P1_B.decl-ast
// Instantiating 'P1/E'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-decl -levitation-dependency=%T/P1_C.ast -levitation-dependency=%T/P1_C.decl-ast -emit-pch %T/P1_E.ast -o %T/P1_E.decl-ast
// Instantiating 'P1/F'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-decl -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast -levitation-dependency=%T/P1_B.ast -levitation-dependency=%T/P1_B.decl-ast -levitation-dependency=%T/P1_C.ast -levitation-dependency=%T/P1_C.decl-ast -levitation-dependency=%T/P1_E.ast -levitation-dependency=%T/P1_E.decl-ast -emit-pch %T/P1_F.ast -o %T/P1_F.decl-ast
// Compiling 'P1/A'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-object -emit-obj %T/P1_A.ast -o %T/P1_A.o
// Compiling 'P1/C'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-object -emit-obj %T/P1_C.ast -o %T/P1_C.o
// Compiling 'P1/B'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-object -emit-obj -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast %T/P1_B.ast -o %T/P1_B.o
// Compiling 'P1/E'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-object -emit-obj -levitation-dependency=%T/P1_C.ast -levitation-dependency=%T/P1_C.decl-ast %T/P1_E.ast -o %T/P1_E.o
// Compiling 'P1/F'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-object -emit-obj -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast -levitation-dependency=%T/P1_C.ast -levitation-dependency=%T/P1_C.decl-ast -levitation-dependency=%T/P1_E.ast -levitation-dependency=%T/P1_E.decl-ast -levitation-dependency=%T/P1_B.ast -levitation-dependency=%T/P1_B.decl-ast %T/P1_F.ast -o %T/P1_F.o
// Compiling source 'main.cpp'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -flevitation-build-object -emit-obj -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast -levitation-dependency=%T/P1_C.ast -levitation-dependency=%T/P1_C.decl-ast -levitation-dependency=%T/P1_E.ast -levitation-dependency=%T/P1_E.decl-ast -levitation-dependency=%T/P1_B.ast -levitation-dependency=%T/P1_B.decl-ast -levitation-dependency=%T/P1_F.ast -levitation-dependency=%T/P1_F.decl-ast %S/main.cpp -o %T/main.o
// RUN:  %clangxx %T/main.o %T/P1_A.o %T/P1_C.o %T/P1_E.o %T/P1_B.o %T/P1_F.o -o %T/app.out
// RUN:  %T/app.out
int main() {
  with (
    auto TestScope = levitation::Test::context()
        .expect("P1::F::f()")
        .expect("P1::B::ff()")
        .expect("P1::A::f()")
        .expect("P1::E::f()")
        .expect("P1::C::f()")
    .open()
  ) {
    P1::F::f();
  }
  return levitation::Test::result();
}
