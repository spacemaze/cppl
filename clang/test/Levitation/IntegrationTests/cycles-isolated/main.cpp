// This is a generated file. Don't edit it.
// Use bash.sh or test-all.sh to generate it again.
// ------------------------------------------------

// Parsing 'P1/Root'...
// RUN:  %clang -cc1 -std=c++17 -xc++ -levitation-build-ast -levitation-sources-root-dir=%S -levitation-deps-output-file=%T/P1_Root.ldeps %S/P1/Root.cppl -o %T/P1_Root.ast
// Parsing 'P1/A'...
// RUN:  %clang -cc1 -std=c++17 -xc++ -levitation-build-ast -levitation-sources-root-dir=%S -levitation-deps-output-file=%T/P1_A.ldeps %S/P1/A.cppl -o %T/P1_A.ast
// Parsing 'P1/B'...
// RUN:  %clang -cc1 -std=c++17 -xc++ -levitation-build-ast -levitation-sources-root-dir=%S -levitation-deps-output-file=%T/P1_B.ldeps %S/P1/B.cppl -o %T/P1_B.ast
// RUN: !  levitation-deps -src-root=%S -build-root=%T -main-file=%S/main.cpp --verbose
