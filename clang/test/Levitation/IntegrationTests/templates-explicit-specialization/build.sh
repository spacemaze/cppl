source ../common.sh

initTests $@

buildPreamble

parse P1/A
parse P1/B

solveDependencies

instantiate P1/A
instantiate P1/B:P1/A

compileAST P1/A
compileAST P1/B:P1/A

compileSrc main.cpp:P1/A,P1/B

link main,P1/A,P1/B

runProgram
