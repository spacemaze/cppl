PREAMBLE_TOGGLE=NO_PREAMBLE

source ../common.sh

initTests $@

parseImport Inputs/A NO_PREAMBLE
buildDecl Inputs/A NO_PREAMBLE
buildObject Inputs/A NO_PREAMBLE
buildObject main:Inputs/A NO_PREAMBLE

link main,Inputs/A

runProgram main.cppl
