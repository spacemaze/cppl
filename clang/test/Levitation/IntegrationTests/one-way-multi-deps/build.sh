source ../common.sh

initTests $@
buildPreamble
parse P1/A
parse P2/B
parse P2/C

solveDependencies

instantiate P1/A
instantiate P2/C
instantiate P2/B:P1/A,P2/C
compileAST P1/A
compileAST P2/C
compileAST P2/B:P1/A,P2/C
compileSrc main.cpp:P1/A,P2/C,P2/B
link main,P1/A,P2/C,P2/B

runProgram