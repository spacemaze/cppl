source ../common.sh

# Cycles test. We only parse it and then trying to solve dependencies.
# A decl depends on B
# B decl depends on A
#
#                .-- decl --.
#                |          |
#                |          v
# Root --decl--> A          B
#                ^          |
#                |          |
#                `-- decl --`

initTests $@
parse P1/Root NO_PREAMBLE
parse P1/A NO_PREAMBLE
parse P1/B NO_PREAMBLE
expectError
solveDependencies
emitScript