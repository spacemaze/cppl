source ../common.sh

initTests $@

buildPreamble

parseImport Inputs/A
parseImport Inputs/B
parseImport Inputs/C
buildModule Inputs/A
buildModule Inputs/B:Inputs/A
buildModule Inputs/C:Inputs/A
buildObject main:Inputs/A,Inputs/B,Inputs/C

link main,Inputs/A,Inputs/B,Inputs/C

runProgram main.cppl
