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
// Parsing 'P1/D'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -levitation-build-ast -levitation-sources-root-dir=%S -levitation-deps-output-file=%T/P1_D.ldeps %S/P1/D.cppl -o %T/P1_D.ast
// RUN:  levitation-deps -src-root=%S -build-root=%T -main-file=%S/main.cpp --verbose
// Instantiating 'P1/A'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-decl -emit-pch %T/P1_A.ast -o %T/P1_A.decl-ast
// Instantiating 'P1/B'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-decl -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast -emit-pch %T/P1_B.ast -o %T/P1_B.decl-ast
// Instantiating 'P1/C'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-decl -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast -emit-pch %T/P1_C.ast -o %T/P1_C.decl-ast
// Instantiating 'P1/D'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-decl -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast -levitation-dependency=%T/P1_B.ast -levitation-dependency=%T/P1_B.decl-ast -levitation-dependency=%T/P1_C.ast -levitation-dependency=%T/P1_C.decl-ast -emit-pch %T/P1_D.ast -o %T/P1_D.decl-ast
// Compiling 'P1/A'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-object -emit-obj %T/P1_A.ast -o %T/P1_A.o
// Compiling 'P1/B'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-object -emit-obj -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast %T/P1_B.ast -o %T/P1_B.o
// Compiling 'P1/C'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-object -emit-obj -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast %T/P1_C.ast -o %T/P1_C.o
// Compiling 'P1/D'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-object -emit-obj -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast -levitation-dependency=%T/P1_B.ast -levitation-dependency=%T/P1_B.decl-ast -levitation-dependency=%T/P1_C.ast -levitation-dependency=%T/P1_C.decl-ast %T/P1_D.ast -o %T/P1_D.o
// Compiling source 'main.cpp'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -flevitation-build-object -emit-obj -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast -levitation-dependency=%T/P1_B.ast -levitation-dependency=%T/P1_B.decl-ast -levitation-dependency=%T/P1_C.ast -levitation-dependency=%T/P1_C.decl-ast -levitation-dependency=%T/P1_D.ast -levitation-dependency=%T/P1_D.decl-ast %S/main.cpp -o %T/main.o
// RUN:  %clangxx %T/main.o %T/P1_A.o %T/P1_B.o %T/P1_C.o %T/P1_D.o -o %T/app.out
// RUN:  %T/app.out
int main() {
  with (
      auto TestScope = levitation::Test::context()
          .expect("P1::D::f()")
          .expect("P1::B::f()")
          .expect("P1::A::f<int>()")
          .expect("P1::C::f()")
          .expect("P1::A::f<int>()")
      .open()
    ) {
      P1::D::f();
    }
    return levitation::Test::result();
}
