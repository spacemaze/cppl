# L-12 regression
# We need special preamble
PREAMBLE_PROJECT_DIR=.
PREAMBLE_BUILD_DIR=$PREAMBLE_PROJECT_DIR/$BUILD_DIR

PREAMBLE_FILE=$PREAMBLE_PROJECT_DIR/preamble.hpp
PREAMBLE_FILE_LIT=preamble.hpp
PREAMBLE_FLAGS="-DCOMPILE_PCH"
PREAMBLE_OBJECT=1

source ../common.sh

# L-12 regression test


# preamble --> B --> C --> D
#          `---------^

# 1. When we parse B, it creates implicit instantiation of Dummy::f<int>()
# 2. When we parse C, it also creates implicit instantiation of Dummy::f<int>() 
# 3. When we instantiate C it already has package instantiated
# version of f<int>,
# and it has non-instantiated version of its own f<int>. So it creates
# another package-instantiated f<int>.
# 4. When we load D, we see two versions of f<int>. We should merge them.

# FIXME: we could probably fix step 3.

# 4. D also requires to load C and it also loads another clone if  

initTests $@

rm $PREAMBLE_PCH 2>/dev/null
buildPreamble

parse P1/B
parse P1/C
parse P1/D

instantiate P1/B
instantiate P1/C:P1/B
instantiate P1/D:P1/B,P1/C

compileAST P1/B
compileAST P1/C:P1/B
compileAST P1/D:P1/B,P1/C

compileSrc main.cpp:P1/B,P1/C,P1/D

link main,preamble,P1/B,P1/C,P1/D

runProgram
