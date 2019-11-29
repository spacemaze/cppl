PREAMBLE_TOGGLE=NO_PREAMBLE

source ../common.sh

initTests $@

buildPreamble
buildDecl Inputs/A
buildDecl Inputs/B
buildObject Inputs/A
buildObject Inputs/B
buildObject main:Inputs/A,Inputs/B

link main,Inputs/A,Inputs/B

runProgram main.cppl
