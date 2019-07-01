// This is a generated file. Don't edit it.
// Use bash.sh or test-all.sh to generate it again.
// ------------------------------------------------

// Parsing 'empty'...
// RUN:  %clang -cc1 -std=c++17 -xc++ -levitation-build-ast -levitation-sources-root-dir=%S -levitation-deps-output-file=%T/empty.ldeps %S/empty.cppl -o %T/empty.ast
// Instantiating 'empty'...
// RUN:  %clang -cc1 -std=c++17 -flevitation-build-decl -emit-pch %T/empty.ast -o %T/empty.decl-ast
