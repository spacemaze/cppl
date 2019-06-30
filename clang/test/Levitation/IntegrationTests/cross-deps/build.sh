source ../common.sh

# A decl doesn't depend on B
# A def depends on B
# B decl depends on A

#    .-- decl --.
#    |          |
#    |          v
#    A          B
#    ^          |
#    |          |
#    `-- def ---`

initTests $@
buildPreamble
parse P1/A
parse P1/B

solveDependencies

instantiate P1/A              # Produces A.decl-ast, should not depend on B decl
instantiate P1/B:P1/A         # Produces B.decl-ast, depends on A decl
compileAST P1/B:P1/A          # Produces B.o, depends on B decl.
compileAST P1/A:P1/A,P1/B     # Produces A.o, depends on B decl, which depends on A decl
compileSrc main.cpp:P1/A,P1/B # Produces main.o, depends on A decl and B decl.
link main,P1/A,P1/B

runProgram