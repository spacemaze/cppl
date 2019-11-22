source ../common.sh

initTests $@

buildPreamble

parseImport Inputs/A
buildDecl Inputs/A
buildObject Inputs/A
buildObject main:Inputs/A

link main,Inputs/A

runProgram main.cppl
