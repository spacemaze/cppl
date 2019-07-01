# Note: I'm not a genius bash scripter. That was supposed to be
# a quick script with short lifetime.
# We keep it, for history and better diagnostics.
#
# This is a functions set which helps to convert bash-based tests into
# llvm-lit tests.
#
# It is used by test-all.sh and build.sh scripts.
# * test-all.sh is supposed to aggregate and run all tests in directory
#   by means of build.sh.
# * build.sh should be present in each test dir. And it tells how to run particular test.
#
# command line format for build.sh:
# build.sh generate <output dir> <test name>
# build.sh execute <test name>

# Set of functions for test runner/creator.
# Two modes are possible:
# 1. "Execute": executes test
# 2. "Generate": generates test for llvm-lit
#
# Example of command, used 'enums' test as a source
#    // RUN:  %clang -cc1 -std=c++17 -xc++ -levitation-build-preamble %S/../preamble.hpp -o %t-preamble.pch
#    // Parsing 'P1/A'...
#    // RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%t-preamble.pch -xc++ -levitation-build-ast -levitation-sources-root-dir=. -levitation-deps-output-file=%t-P1_A.ldeps %S/P1/A.cppl -o %t-P1_A.ast
#    // Instantiating 'P1/A'...
#    // RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%t-preamble.pch -flevitation-build-decl -emit-pch %t-P1_A.ast -o %t-P1_A.decl-ast
#    // Compiling 'P1/A'...
#    // RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%t-preamble.pch -flevitation-build-object -emit-obj %t-P1_A.ast -o %t-P1_A.o
#    // Compiling source 'main.cpp'...
#    // RUN:  %clang -cc1 -std=c++17 -levitation-preamble=%t-preamble.pch -xc++ -flevitation-build-object -emit-obj -levitation-dependency=%t-P1_A.ast -levitation-dependency=%t-P1_A.decl-ast %s -Dmain_cpp -o %t-main.o
#    // RUN:  %clangxx %t-main.o %t-P1_A.o -o %t-enums.out
#    // RUN:  %t-enums.out


CXX_EXECUTE=/Users/stepan/projects/shared/toolchains/llvm.git.darwin-debug-x86_64/bin/clang
CXX_GENERATE=%clang

# CXX will be set during setBuildModeXXXX calls
CXX=

LEVITATION_DEPS_EXECUTE=/Users/stepan/projects/shared/toolchains/llvm.git.darwin-debug-x86_64/bin/levitation-deps
LEVITATION_DEPS_GENERATE=levitation-deps

# LEVITATION_DEPS will be set during setBuildModeXXXX calls
LEVITATION_DEPS=

LINKER_EXECUTE=/Users/stepan/projects/shared/toolchains/llvm.git.darwin-debug-x86_64/bin/clang++
LINKER_GENERATE=%clangxx

# LINKER will be set during setBuildModeXXXX calls
LINKER=

FILECHECK_EXECUTE=/Users/stepan/projects/shared/toolchains/llvm.git.darwin-debug-x86_64/bin/FileCheck
FILECHECK_GENERATE=FileCheck

# FILECHECK will be set during setBuildModeXXXX calls
FILECHECK=

# CXX="CXX"
NEWLINE="echo"
CAT=cat
CP=cp

TEMP_DIR=/tmp

# Build dir for execute mode
BUILD_DIR_EXECUTE=build

BUILD_DIR=$BUILD_DIR_EXECUTE

PROJECT_DIR=.

APP_NAME_EXECUTE=app.out
APP_NAME_GENERATE=app.out # same-same
APP_NAME=

TESTS_DIR=$PROJECT_DIR/..

MAIN_SRC=main.cpp
MAIN_SRC_IN=$MAIN_SRC.in

# MAIN_SRC_ORIGIN_PATH will be set during setBuildModeXXXX calls
MAIN_SRC_ORIGIN_PATH=

if [ -z "$PREAMBLE_PROJECT_DIR" ]; then
  PREAMBLE_PROJECT=.

  PREAMBLE_PROJECT_DIR=$TESTS_DIR/$PREAMBLE_PROJECT
  PREAMBLE_PROJECT_DIR_REL=../$PREAMBLE_PROJECT

  PREAMBLE_BUILD_DIR=$PREAMBLE_PROJECT_DIR/$BUILD_DIR

  PREAMBLE_FILE=$PREAMBLE_PROJECT_DIR/preamble.hpp
  PREAMBLE_FILE_LIT=../preamble.hpp
  PREAMBLE_PCH=$PREAMBLE_BUILD_DIR/preamble.pch

  # Custom preamble flags, may be set in build.sh
  PREAMBLE_FLAGS=

  PREAMBLE_OBJECT=0
fi

BUILD_MODE_EXECUTE=execute
BUILD_MODE_GENERATE=generate
BUILD_MODE_GENERATE_MAIN=generate-main

if [ -z "$BUILD_MODE" ]; then
  BUILD_MODE=$BUILD_MODE_EXECUTE
fi

# GENERATED_OUTPUT params will be set during setBuildModeXXXX calls
GENERATE_OUTPUT_DIR=
GENERATED_TEST_NAME=
GENERATED_OUTPUT=
GENERATED_OUTPUT_COMMANDS=
GENERATE_ONLY_MAIN=0

# FLAGS will be set during setupFlags
FLAGS=

#Warning: don't use $CC_FLAGS directly, use $CC_FLAGS instead.
CC_FLAGS_EXECUTE="-cc1 -std=c++17 -stdlib=libstdc++"
CC_FLAGS_GENERATE="-cc1 -std=c++17"

# CC_FLAGS params will be set during setBuildModeXXXX calls
CC_FLAGS=

LINKER_FLAGS_EXECUTE="-stdlib=libstdc++"
LINKER_FLAGS_GENERATE=""

# LINKER_FLAGS params will be set during setBuildModeXXXX calls
LINKER_FLAGS=

LOG_LEVEL_SILENT=0
LOG_LEVEL_ERROR=1
LOG_LEVEL_INFO=2
LOG_LEVEL_DEBUG=3
LOG_LEVEL_DUMP=4

function setLogsLevelSilent {
  export LEVITATION_TESTS_LOG_LEVEL=$LOG_LEVEL_SILENT
}
function setLogsLevelError {
  export LEVITATION_TESTS_LOG_LEVEL=$LOG_LEVEL_ERROR
}
function setLogsLevelInfo {
  export LEVITATION_TESTS_LOG_LEVEL=$LOG_LEVEL_INFO
}
function setLogsLevelDebug {
  export LEVITATION_TESTS_LOG_LEVEL=$LOG_LEVEL_DEBUG
}
function setLogsLevelDump {
  export LEVITATION_TESTS_LOG_LEVEL=$LOG_LEVEL_DUMP
}

BOLD='\033[01m'
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

LOG_LEVEL_DEFAULT=$LOG_LEVEL_DUMP

function doIfLevelGE {
    REQUIRED_LEVEL=$1
    THENCMD=$2
    ELSECMD=$3

    if [ -z "$LEVITATION_TESTS_LOG_LEVEL" ]; then
        LEVITATION_TESTS_LOG_LEVEL=$LOG_LEVEL_DEFAULT
    fi

    if [ "$LEVITATION_TESTS_LOG_LEVEL" -ge "$REQUIRED_LEVEL" ]; then
        $THENCMD
    else
        $ELSECMD
    fi
}
function echoIfLevelGE {
    REQUIRED_LEVEL=$1
    COLOR=${2}
    ECHO_ARGS="${@:3}"

    if [ -z "$LEVITATION_TESTS_LOG_LEVEL" ]; then
        LEVITATION_TESTS_LOG_LEVEL=$LOG_LEVEL_DEFAULT
    fi

    if [ "$LEVITATION_TESTS_LOG_LEVEL" -ge "$REQUIRED_LEVEL" ]; then
        echo -e "${COLOR}$ECHO_ARGS${NC}"
    fi
}

function echoIfSilent {
  echoIfLevelGE $LOG_LEVEL_SILENT ${BLUE} $@
}

function echoIfError {
  echoIfLevelGE $LOG_LEVEL_ERROR ${RED} $@
}

function echoIfInfo {
  echoIfLevelGE $LOG_LEVEL_INFO ${GREEN} $@
}

function echoIfDebug {
  echoIfLevelGE $LOG_LEVEL_DEBUG ${NC} $@
}

function echoIfDump {
  echoIfLevelGE $LOG_LEVEL_DUMP $NC $@
}

EXPECT_ERROR=0

function expectError {
    EXPECT_ERROR=1
}

function expectSuccess {
    EXPECT_ERROR=0
}

function runCommand {
    CMD=""
    for ARG in "$@"
    do
        CMD="$CMD $ARG"
    done

    echoIfDump $CMD

    if [ $BUILD_MODE == $BUILD_MODE_GENERATE ]; then
      if [ $EXPECT_ERROR -ne 1 ]; then
        echo "// RUN: $CMD" >> $GENERATED_OUTPUT_COMMANDS
      else
        echo "// RUN: ! $CMD" >> $GENERATED_OUTPUT_COMMANDS
      fi
      return 0
    fi

    STDOUTDEV=`doIfLevelGE $LOG_LEVEL_INFO "printf /dev/stdout" "printf /dev/null"`
    STDERRDEV=`doIfLevelGE $LOG_LEVEL_ERROR "printf /dev/stderr" "printf /dev/null"`
    $CMD 1>$STDOUTDEV 2>$STDERRDEV

    RUN_COMMAND_RES=$?

    if [ $RUN_COMMAND_RES -ne 0 ]; then
        if [ $EXPECT_ERROR -ne 1 ]; then
          echoIfError "Failed with result $RUN_COMMAND_RES. Terminating..."
          exit 1
        else
          echoIfDebug "Failed with result $RUN_COMMAND_RES as expected."
        fi
    else
        if [ $EXPECT_ERROR -ne 1 ]; then
          echoIfDebug "Successfull."
        else
          echoIfError "Successfull, but failure with " \
                      "result $EXPECT_ERROR expected. Terminating..."
          exit 1
        fi
    fi
}

function runCommandAndCheck {

    CHECKSFILE=$1

    CMD="${@:2}"

    echoIfDump "File check, checks file: $CHECKSFILE"
    echoIfDump $CMD

    STDOUTDEV=`doIfLevelGE $LOG_LEVEL_INFO "printf /dev/stdout" "printf /dev/null"`
    STDERRDEV=`doIfLevelGE $LOG_LEVEL_ERROR "printf /dev/stderr" "printf /dev/null"`

    if [ $BUILD_MODE == $BUILD_MODE_GENERATE ]; then
      echo "// RUN: $CMD | $FILECHECK $CHECKSFILE" >> $GENERATED_OUTPUT_COMMANDS
      return 0
    fi

    STDOUT=`$CMD 2>$STDERRDEV`

    RUN_COMMAND_RES=$?

    if [ $RUN_COMMAND_RES -ne 0 ]; then
       echoIfError "Program failed. Terminating..."
       return 1
    fi

    if [ -z "$STDOUT" ]; then
       echoIfError "Program output is empty. Terminating..."
       return 1
    fi

    printf "$STDOUT" | $FILECHECK $CHECKSFILE 1>$STDOUTDEV 2>$STDERRDEV

    if [ $RUN_COMMAND_RES -ne 0 ]; then
       echoIfError "Checks Failed. Terminating..."
    else
       echoIfDebug "Successfull."
    fi
}

function dumpIfNotEmpty {
    if [ ! -z "$2" ]; then
        echoIfDump "$1"
        echoIfDump "$2"
        echoIfDump
    fi
}

function createDir {
    TITLE=$1
    DIR=$2

    if [[ -f "$DIR" ]]
    then
        echoIfError "Can't use $TITLE '$DIR', because it's a file. Terminating."
        exit 1
    fi

    if [[ ! -d "$DIR" ]]
    then
        echoIfDump "Creating '$TITLE' directory, path:"
        echoIfDump "-- '$PWD/$DIR'"

        mkdir -p $DIR
    fi
}

function cleanBuildDir {
    rm -Rf $BUILD_DIR/*
}

function cleanApp {
    rm $APP_NAME 2>/dev/null
}

function createBuildDir {
    echoIfInfo "Checking build directory..."

    createDir "Build directory" $BUILD_DIR

    if [[ -d "$BUILD_DIR" ]]
    then
        if find "$BUILD_DIR" -mindepth 1 -print -quit 2>/dev/null | grep -q .; then
            echoIfInfo "Build directory is not empty, cleaning..."
            cleanBuildDir
        fi
    fi
}

function createDirFor {
  for FILE in "$@"
  do
    DIR=$(dirname "${FILE}")
    createDir "Directory for '$FILE'" $DIR
  done
}

function copyFile {
  SRC=$1
  DEST=$2

  if [ -z "$SRC" ] || ! [ -f $SRC ]; then
    return 0;
  fi

  echoIfDump "Copying $SRC to $DEST..."
  # createDirFor $DEST
  $CP $SRC $DEST
}

function appendFile {
  SRC=$1
  DEST=$2

  if [ -z "$SRC" ] || ! [ -f $SRC ]; then
    return 0;
  fi

  echoIfDump "Appending $SRC to $DEST..."
  $CAT $SRC >> $DEST
}

function setupFlags {
    COMMON_FLAGS="$CC_FLAGS $PREAMBLE_PARAM"
    PREAMBLE_TOGGLE=$1
    ADDITIONAL_FLAGS=$2
    # echoIfDump "Setting flags..."
    if [ "$PREAMBLE_TOGGLE" == "NO_PREAMBLE" ]; then
        # echoIfDump "Preamble=OFF flags: $ADDITIONAL_FLAGS"
        FLAGS="$CC_FLAGS $ADDITIONAL_FLAGS"
    else
        # echoIfDump "Preamble=ON flags: $ADDITIONAL_FLAGS"
        FLAGS="$COMMON_FLAGS $ADDITIONAL_FLAGS"
    fi
}

function setBuildModeExecute {
  BUILD_MODE=$BUILD_MODE_EXECUTE
  BUILD_DIR=$BUILD_DIR_EXECUTE
  CXX=$CXX_EXECUTE
  LINKER=$LINKER_EXECUTE
  LEVITATION_DEPS=$LEVITATION_DEPS_EXECUTE
  FILECHECK=$FILECHECK_EXECUTE

  CC_FLAGS=$CC_FLAGS_EXECUTE

  LINKER_FLAGS=$LINKER_FLAGS_EXECUTE

  PREAMBLE_SRC=$PREAMBLE_FILE

  APP_NAME=./$APP_NAME_EXECUTE
}

function setBuildModeGenerate {

  GENERATED_TEST_NAME=$2
  GENERATE_OUTPUT_DIR=$1/$2

  if [ -z "$GENERATE_OUTPUT_DIR" ] || [ -z "$GENERATED_TEST_NAME" ]; then
    echoIfError "setBuildModeGenerate should accept at lest two parameters:"
    echoIfError "  setBuildModeGenerate <output-dir> <test-name>"
    echoIfError "Terminating..."
    exit 1
  fi

  BUILD_MODE=$BUILD_MODE_GENERATE
  CXX=$CXX_GENERATE
  LINKER=$LINKER_GENERATE
  LEVITATION_DEPS=$LEVITATION_DEPS_GENERATE
  FILECHECK=$FILECHECK_GENERATE

  CC_FLAGS=$CC_FLAGS_GENERATE

  LINKER_FLAGS=$LINKER_FLAGS_GENERATE

  GENERATED_OUTPUT=$GENERATE_OUTPUT_DIR/$MAIN_SRC

  createDirFor $GENERATED_OUTPUT

  BUILD_DIR=%T

  GENERATED_OUTPUT_COMMANDS="$TEMP_DIR/$GENERATED_TEST_NAME.commands"

  PREAMBLE_SRC=%S/$PREAMBLE_FILE_LIT

  APP_NAME=%T/$APP_NAME_GENERATE

  if [ -f $MAIN_SRC_IN ]; then
    MAIN_SRC_ORIGIN_PATH=$PROJECT_DIR/$MAIN_SRC_IN
#  elif [ -f $MAIN_SRC ]; then
#    MAIN_SRC_ORIGIN_PATH=$PROJECT_DIR/$MAIN_SRC
  else
    echoIfInfo "'$MAIN_SRC_IN' doesn't exist in test '$TEST_NAME'."
  fi
}

function getSourceRootCommandLineParam {
    if [ "$BUILD_MODE" == "$BUILD_MODE_EXECUTE" ]; then
      printf $PROJECT_DIR
    else
      printf %%S
    fi
}
function getModulePathRel {
  MODULE=$1

  EXT="$([[ "$MODULE" = *.* ]] && echo ".${MODULE##*.}" || echo '')"

  if [ -z "$EXT" ]; then
      printf $MODULE.cppl
  else
      printf $MODULE
  fi
}

function getModuleDestinationRel {
  MODULE=$1

  EXT="$([[ "$MODULE" = *.* ]] && echo ".${MODULE##*.}" || echo '')"

  if [ -z "$EXT" ]; then
      printf $MODULE.cppl
  elif [ "$EXT" == ".cpp" ] && [ $MODULE != $MAIN_SRC ]; then
      printf ${MODULE%.*}.cpp_
  else
      printf $MODULE
  fi
}

function getModulePath {
  printf $PROJECT_DIR/$(getModulePathRel $1)
}

function getSrcFileName {
  MODULE=$1
  if [ "$BUILD_MODE" == "$BUILD_MODE_EXECUTE" ]; then
    printf "$(getModulePath $MODULE)"
  else
    printf "%%S/$(getModuleDestinationRel $MODULE)"
  fi
}

function getBuildedFileName {
  FILENAME=$1
  BUILD_DIR_FOR_PRINTF=${BUILD_DIR/\%/%%}
  if [ "$BUILD_MODE" == "$BUILD_MODE_EXECUTE" ]; then
    printf "$BUILD_DIR_FOR_PRINTF/$FILENAME"
  else
    printf "$BUILD_DIR_FOR_PRINTF/$(echo $FILENAME | tr / _)"
  fi
}

function getClang {
  if [ "$BUILD_MODE" == "$BUILD_MODE_EXECUTE" ]; then
    printf "$CXX"
  else
    printf "clang_cc1"
  fi
}

function getCCFlags {
  if [ "$BUILD_MODE" == "$BUILD_MODE_EXECUTE" ]; then
    printf -- "-cc1 $CC_FLAGS"
  else
    printf -- "$CC_FLAGS"
  fi
}

function registerSource {
  ORIGINAL_SOURCE_PATH=$1
  DESTINATION_SOURCE_PATH=$GENERATE_OUTPUT_DIR/$2

  createDirFor $DESTINATION_SOURCE_PATH

  echoIfDump "Registering source $ORIGINAL_SOURCE_PATH at $DESTINATION_SOURCE_PATH..."
  cp $ORIGINAL_SOURCE_PATH $DESTINATION_SOURCE_PATH
}


function registerModule {
  if [ "$BUILD_MODE" == "$BUILD_MODE_GENERATE" ] && \
     [ "$GENERATE_ONLY_MAIN" == "0" ]; then
    echoIfDump "Registering module $MODULE..."
    MODULE_SOURCE=$(getModulePath $MODULE)
    MODULE_SOURCE_REL=$(getModuleDestinationRel $MODULE)
    registerSource $MODULE_SOURCE $MODULE_SOURCE_REL
  fi
}

function registerPreamble {
  if [ "$BUILD_MODE" == "$BUILD_MODE_GENERATE" ]; then
    if ! [ -f $GENERATE_OUTPUT_DIR/$PREAMBLE_FILE_LIT ]; then
      registerSource $PREAMBLE_FILE $PREAMBLE_FILE_LIT
    fi
  fi
}

function buildPreamble {

    # registerPreamble

    echoIfDebug "Building preamble AST $PREAMBLE_SRC => $PREAMBLE_PCH"

    if [[ -f "$PCHFILE" && $BUILD_MODE" == "$BUILD_MODE_EXECUTE ]]
    then
        echoIfDebug "Preamble exists, skipping..."
    else
        runCommand $CXX $CC_FLAGS -xc++ \
                   -levitation-build-preamble $PREAMBLE_FLAGS \
                   $PREAMBLE_SRC -o $PREAMBLE_PCH

        if [ "$PREAMBLE_OBJECT" == "1" ]; then
          PREAMBLE_OBJ=$(getBuildedFileName preamble.o)
          runCommand $CXX $CC_FLAGS -xc++ $PREAMBLE_SRC -emit-obj -o $PREAMBLE_OBJ
        fi
    fi
    echoIfDebug
}

function parse {
    MODULE=$1

    BUILD_AST_FLAGS="-xc++ -levitation-build-ast"
    BUILD_AST_FLAGS="$BUILD_AST_FLAGS -levitation-sources-root-dir=$(getSourceRootCommandLineParam)"

    setupFlags "$2" "$BUILD_AST_FLAGS"

    # echoIfDump "Flags: $FLAGS"

    registerModule $MODULE

    SRCFILE="$(getSrcFileName $MODULE)"
    ASTFILE="$(getBuildedFileName $MODULE.ast)"
    DEPSFILE="$(getBuildedFileName $MODULE.ldeps)"

    if [ "$BUILD_MODE" == "$BUILD_MODE_EXECUTE" ]; then
      createDirFor $ASTFILE $DEPSFILE
    fi

    echoIfDebug "Parsing '$MODULE'..."

    if [ "$BUILD_MODE" == "$BUILD_MODE_GENERATE" ]; then
        echo "// Parsing '$MODULE'..." >> $GENERATED_OUTPUT_COMMANDS
    fi

    runCommand $CXX $FLAGS "-levitation-deps-output-file=$DEPSFILE" $SRCFILE "-o" $ASTFILE
    echoIfDebug
}

function instantiate {
    # Should be in format:
    # MODULE:DEP1,DEP2,...
    MODULE_WITH_DEPS=$1
    IFS=:;
    read MODULE DEPS <<< "$MODULE_WITH_DEPS"
    IFS=" "

    echoIfDebug "Instantiating '$MODULE'..."
    dumpIfNotEmpty "Dependencies:" $DEPS

    if [ "$BUILD_MODE" == "$BUILD_MODE_GENERATE" ]; then
        echo "// Instantiating '$MODULE'..." >> $GENERATED_OUTPUT_COMMANDS
    fi

    DEP_FLAGS=""
    for DEP in $(echo $DEPS | sed "s/,/ /g")
    do
        DEP_FLAGS="$DEP_FLAGS -levitation-dependency=$(getBuildedFileName $DEP.ast)"
        DEP_FLAGS="$DEP_FLAGS -levitation-dependency=$(getBuildedFileName $DEP.decl-ast)"
    done

    INSTANTIATE_FLAGS="-flevitation-build-decl $DEP_FLAGS -emit-pch"

    setupFlags "$2" "$INSTANTIATE_FLAGS"

    INITIAL_AST_FILE="$(getBuildedFileName $MODULE.ast)"
    DECL_AST_FILE="$(getBuildedFileName $MODULE.decl-ast)"

    if [ "$BUILD_MODE" == "$BUILD_MODE_EXECUTE" ]; then
      createDirFor $DECL_AST_FILE
    fi

    runCommand $CXX $FLAGS $INITIAL_AST_FILE "-o" $DECL_AST_FILE
    echoIfDebug
}

# CXX -cc1 -std=c++17 -stdlib=libstdc++ -flevitation-build-object \
# -levitation-preamble=./test-project/preamble.pch ./test-project/P1/A.ast \
# -emit-obj -o ./test-project/P1/A.o
function compileAST {

    COMPILE_OBJECT_FLAGS="-flevitation-build-object -emit-obj"

    setupFlags "$2" "$COMPILE_OBJECT_FLAGS"

    # Should be in format:
    # MODULE:DEP1,DEP2,...
    MODULE_WITH_DEPS=$1
    IFS=:;
    read MODULE DEPS <<< "$MODULE_WITH_DEPS"
    IFS=" "

    echoIfDebug "Compiling '$MODULE'..."
    dumpIfNotEmpty "Dependencies:" $DEPS

    if [ "$BUILD_MODE" == "$BUILD_MODE_GENERATE" ]; then
        echo "// Compiling '$MODULE'..." >> $GENERATED_OUTPUT_COMMANDS
    fi

    DEP_FLAGS=""
    for DEP in $(echo $DEPS | sed "s/,/ /g")
    do
        DEP_FLAGS="$DEP_FLAGS -levitation-dependency=$(getBuildedFileName $DEP.ast)"
        DEP_FLAGS="$DEP_FLAGS -levitation-dependency=$(getBuildedFileName $DEP.decl-ast)"
    done

    INITIAL_AST_FILE="$(getBuildedFileName $MODULE.ast)"
    OBJECT_FILE="$(getBuildedFileName $MODULE.o)"

    if [ "$BUILD_MODE" == "$BUILD_MODE_EXECUTE" ]; then
      createDirFor $OBJECT_FILE
    fi

    runCommand $CXX $FLAGS $DEP_FLAGS $INITIAL_AST_FILE "-o" $OBJECT_FILE
    echoIfDebug
}

function compileSrc {

    COMPILE_OBJECT_FLAGS="-xc++ -flevitation-build-object -emit-obj"

    setupFlags "$2" "$COMPILE_OBJECT_FLAGS"

    # Should be in format:
    # MODULE(with extension):DEP1,DEP2,...
    MODULE_WITH_DEPS=$1
    IFS=:
    read MODULE DEPS <<< "$MODULE_WITH_DEPS"

    IFS=.
    read MODULE_WITHOUT_EXT MODULE_EXT <<< "$MODULE"
    IFS=" "

    echoIfDebug "Compiling source '$MODULE'..."
    dumpIfNotEmpty "Dependencies:" $DEPS

    if [ "$BUILD_MODE" == "$BUILD_MODE_GENERATE" ]; then
      echo "// Compiling source '$MODULE'..." >> $GENERATED_OUTPUT_COMMANDS
    fi

    DEP_FLAGS=""
    for DEP in $(echo $DEPS | sed "s/,/ /g")
    do
      DEP_FLAGS="$DEP_FLAGS -levitation-dependency=$(getBuildedFileName $DEP.ast)"
      DEP_FLAGS="$DEP_FLAGS -levitation-dependency=$(getBuildedFileName $DEP.decl-ast)"
    done

    # Main file will be generated, and shall not be registered
    # as regular module.
    if [ "$MODULE" != "$MAIN_SRC" ]; then
      registerModule $MODULE
    fi

    SRCFILE="$(getSrcFileName $MODULE)"
    OBJECT_FILE="$(getBuildedFileName $MODULE_WITHOUT_EXT.o)"

    if [ "$BUILD_MODE" == "$BUILD_MODE_EXECUTE" ]; then
      createDirFor $OBJECT_FILE
    fi

    runCommand $CXX $FLAGS $DEP_FLAGS $SRCFILE "-o" $OBJECT_FILE
    echoIfDebug
}

function compileMainSrc {
    compileSrc $MAIN_SRC:$@
}

function link {

    OBJECTS=$1

    echoIfDebug "Linking objects: $OBJECTS"

    LINK_OBJECTS=""
    for MODULE in $(echo $OBJECTS | sed "s/,/ /g")
    do
        LINK_OBJECT=$(getBuildedFileName $MODULE.o)
        LINK_OBJECTS="$LINK_OBJECTS $LINK_OBJECT"
    done

    runCommand $LINKER $LINKER_FLAGS $LINK_OBJECTS -o $APP_NAME
    LINK_RES=$RUN_COMMAND_RES

    echoIfDebug

    return $LINK_RES
}

function solveDependencies {
    echoIfInfo "Solving dependencies:"
    SRCROOT=
    if [ "$BUILD_MODE" == "$BUILD_MODE_EXECUTE" ]; then
      SRCROOT=$PROJECT_DIR
    else
      SRCROOT=$(getSourceRootCommandLineParam)
    fi

    MAINFILE="$(getSrcFileName $MAIN_SRC)"

    runCommand $LEVITATION_DEPS -src-root=$SRCROOT -build-root=$BUILD_DIR -main-file=$MAINFILE --verbose
}

function emitScript {
  if [ "$BUILD_MODE" == "$BUILD_MODE_GENERATE" ]; then
    appendFile "$GENERATED_OUTPUT_COMMANDS" "$GENERATED_OUTPUT"
    appendFile "$MAIN_SRC_ORIGIN_PATH" "$GENERATED_OUTPUT"

    if [ "$GENERATE_ONLY_MAIN" == "0" ]; then
      copyFile "$MAIN_SRC_ORIGIN_PATH" "$GENERATE_OUTPUT_DIR/$MAIN_SRC_IN"
    fi
  fi
}

function runProgram {
    echoIfInfo "Running program:"
    runCommand $APP_NAME
    emitScript
}

function runProgramAndCheck {
    echoIfInfo "Running program:"
    runCommandAndCheck $1 $APP_NAME
    emitScript
}

function printResult {
    RES=$1
    TEST_NAME=$2

    if [ $RES -ne 0 ]; then
        echoIfSilent "${RED}${BOLD}Test '$TEST_NAME' has errors"
    else
        echoIfSilent "${GREEN}${BOLD}'$TEST_NAME' successfull!"
    fi
}

function initTests {

  if [ "$#" == "0" ]; then
    echoIfError "initTests should accept at least two arguments: <mode> and <test name>"
    echoIfError "Terminating..."
    exit 1
  fi

  echoIfDump "Command line arguments:"
  echoIfDump "$@"

  if [ "$1" == "$BUILD_MODE_GENERATE" ] || \
     [ "$1" == "$BUILD_MODE_GENERATE_MAIN" ]; then

    echoIfDump "Generating llvm-lit tests..."

    OUTPUT_DIR=$2
    createDir "Tests output" $OUTPUT_DIR

    TEST_NAME=$3

    setBuildModeGenerate $OUTPUT_DIR $TEST_NAME

    if [ "$1" == "$BUILD_MODE_GENERATE_MAIN" ]; then
      GENERATE_ONLY_MAIN=1
    fi

    rm $GENERATED_OUTPUT 2>/dev/null
    rm $GENERATED_OUTPUT_COMMANDS 2>/dev/null

    echo   "// This is a generated file. Don't edit it." \
    >> $GENERATED_OUTPUT

    if ! [ -z "$MAIN_SRC_ORIGIN_PATH" ]; then
      echo "// Edit $MAIN_SRC_IN and use bash.sh or test-all.sh" \
      >> $GENERATED_OUTPUT
      echo "// to generate it again." \
      >> $GENERATED_OUTPUT
    else
      echo "// Use bash.sh or test-all.sh to generate it again." \
      >> $GENERATED_OUTPUT
    fi

    echo   "// ------------------------------------------------" \
    >> $GENERATED_OUTPUT
    echo "" >> $GENERATED_OUTPUT

  else
    echoIfDump "Running tests..."
    setBuildModeExecute
    createBuildDir
  fi

  PREAMBLE_PCH=$(getBuildedFileName preamble.pch)
  PREAMBLE_OBJ=$(getBuildedFileName preamble.o)
  PREAMBLE_PARAM="-levitation-preamble=$PREAMBLE_PCH"
}
