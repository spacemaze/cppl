source ../common.sh

# Check that implicit template specializations works fine.


#           B-- def --.
#           ^          |
#           |          v
# A -decl-->|          D
#           |          ^
#           v          |
#           C-- def ---`

# Template is defined at A
# B and C depends on A and produces equal implicit instantiations of A
# D uses B anc D, so their implicit instantiations should be merged.

initTests $@

buildPreamble

parse P1/A
parse P1/B
parse P1/C
parse P1/D

solveDependencies

instantiate P1/A
instantiate P1/B:P1/A
instantiate P1/C:P1/A
instantiate P1/D:P1/A,P1/B,P1/C

compileAST P1/A
compileAST P1/B:P1/A
compileAST P1/C:P1/A
compileAST P1/D:P1/A,P1/B,P1/C

compileSrc main.cpp:P1/A,P1/B,P1/C,P1/D

link main,P1/A,P1/B,P1/C,P1/D

runProgram
