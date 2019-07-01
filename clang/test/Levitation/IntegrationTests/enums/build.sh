source ../common.sh

initTests $@

buildPreamble

parse P1/A
instantiate P1/A
compileAST P1/A

compileMainSrc P1/A

link main,P1/A

runProgram main.cpp
