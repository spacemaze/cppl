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
  with (
    auto TestScope = levitation::Test::context()
        .expect("P1::A::A(float)")
        .expect("P1::A::A(double)")
        .expect("P1::A::f<float>()")
        .expect("P1::A::f<double>()")
        .expect("P1::A<T>::operator<<(U)")
    .open()
  ) {
    P1::A a((float)0.1);
    P1::A b((double)0.1);
    a.f<float>();
    b.f<double>();
    a << 1;
  }
  return levitation::Test::result();
}
