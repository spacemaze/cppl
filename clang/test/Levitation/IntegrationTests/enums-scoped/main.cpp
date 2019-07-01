// This is a generated file. Don't edit it.
// Edit main.cpp.in and use bash.sh or test-all.sh
// to generate it again.
// ------------------------------------------------

// RUN:  %clang -cc1 -std=c++17 -xc++ -levitation-build-preamble %S/../preamble.hpp -o %T/preamble.pch
// Parsing 'P1/A'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -levitation-build-ast -levitation-sources-root-dir=%S -levitation-deps-output-file=%T/P1_A.ldeps %S/P1/A.cppl -o %T/P1_A.ast
// Parsing 'P1/B'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -levitation-build-ast -levitation-sources-root-dir=%S -levitation-deps-output-file=%T/P1_B.ldeps %S/P1/B.cppl -o %T/P1_B.ast
// Instantiating 'P1/A'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-decl -emit-pch %T/P1_A.ast -o %T/P1_A.decl-ast
// Instantiating 'P1/B'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-decl -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast -emit-pch %T/P1_B.ast -o %T/P1_B.decl-ast
// Compiling 'P1/A'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-object -emit-obj %T/P1_A.ast -o %T/P1_A.o
// Compiling 'P1/B'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -flevitation-build-object -emit-obj -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast %T/P1_B.ast -o %T/P1_B.o
// Compiling source 'main.cpp'...
// RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%T/preamble.pch -xc++ -flevitation-build-object -emit-obj -levitation-dependency=%T/P1_A.ast -levitation-dependency=%T/P1_A.decl-ast -levitation-dependency=%T/P1_B.ast -levitation-dependency=%T/P1_B.decl-ast %S/main.cpp -o %T/main.o
// RUN:  %clangxx %T/main.o %T/P1_A.o %T/P1_B.o -o %T/app.out
// RUN:  %T/app.out
int main() {
  with (
    auto TestScope = levitation::Test::context()
        .expect('A')
        .expect('B')
        .expect('B')
        .expect('C')
    .open()
  ) {
    levitation::Test::context()
        << (char) P1::A::V0
        << (char) P1::A::V1
        << (char) P1::B::V0
        << (char) P1::B::V1;
  }
  return levitation::Test::result();
}
