source ../common.sh

initTests $@

buildPreamble

parse P1/A
parse P1/B
parse P1/C
parse P1/E
parse P1/F

solveDependencies

instantiate P1/A
instantiate P1/C
instantiate P1/B:P1/A
instantiate P1/E:P1/C
instantiate P1/F:P1/A,P1/B,P1/C,P1/E

compileAST P1/A
compileAST P1/C
compileAST P1/B:P1/A
compileAST P1/E:P1/C
compileAST P1/F:P1/A,P1/C,P1/E,P1/B

compileSrc main.cpp:P1/A,P1/C,P1/E,P1/B,P1/F

link main,P1/A,P1/C,P1/E,P1/B,P1/F

runProgram