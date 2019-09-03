
CMAKE=cmake
MAKE=make
LEVITATION_DEPS=/Users/stepan/projects/shared/toolchains/llvm.git.darwin-debug-x86_64/bin/levitation-deps
GENERATOR="Unix Makefiles"

SOURCES_DIR=".."
TOOLCHAIN="../../cmake-builer/toolchains/default/toolchain.cmake"
DEPENDENCIES_TARGET="levitation-parse"
COMPILE_TARGET="all"


function initProject() {
    $CMAKE -G"$GENERATOR" -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN $SOURCES_DIR
    return $?
}

function updateProject() {
    $CMAKE -G"$GENERATOR" $SOURCES_DIR 1>/dev/null
    return $?
}

function parse() {
    $MAKE $DEPENDENCIES_TARGET
    return $?
}

function solveDependencies() {
    $LEVITATION_DEPS -src-root=$SOURCES_DIR -build-root=. -main-file=main.cpp
}

function compile() {
    $MAKE $COMPILE_TARGET
    return $?
}

if [ "$1" == "INIT" ]; then
  initProject
else
  updateProject
fi

if [ $? -ne 0 ]; then
  exit $?
fi

echo "Parsing..." &&
parse &&
solveDependencies &&
updateProject &&
echo "Compiling..." &&
compile