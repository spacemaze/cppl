source ../common.sh

initTests $@

buildPreamble

parseImport Inputs/A
parseImport Inputs/B
buildModule Inputs/A
buildModule Inputs/B
buildObject main:Inputs/A,Inputs/B

link main,Inputs/A,Inputs/B

runProgram main.cppl
