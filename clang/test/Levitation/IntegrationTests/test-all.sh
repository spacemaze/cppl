# Command line format:
# test-all.sh generate <output-dir>
# test-all.sh execute

source common.sh

setLogsLevelSilent

function generate {
  bash build.sh generate $OUTPUT_DIR $1
  cp build.sh $OUTPUT_DIR/$1
}

function generate_main {
  bash build.sh generate-main $OUTPUT_DIR $1
}

function execute {
  bash build.sh execute
}

WHAT_WE_ARE_DOING=

if [ "$1" == "$BUILD_MODE_GENERATE" ]; then

  OUTPUT_DIR=$2

  if [[ "${OUTPUT_DIR:0:1}" != "/" ]] ; then
    OUTPUT_DIR=$PWD/$OUTPUT_DIR
  fi

  if [ -z $OUTPUT_DIR ]; then
    echoIfError "Output directory parameter should not be empty"
    echoIfError "Generate mode command format: tests-all.sh generate <output-dir>"
    exit 1
  fi

  echoIfDump "Cleaning output directory:"
  echoIfDump "-- $OUTPUT_DIR"
  rm -Rf $OUTPUT_DIR

  createDir "Tests output" $OUTPUT_DIR

  if [[ -d "$OUTPUT_DIR" ]]
  then
    if find "$OUTPUT_DIR" -mindepth 1 -print -quit 2>/dev/null | grep -q .; then
      echoIfInfo "Tests directory is not empty, cleaning..."
      cleanBuildDir
    fi
  fi

  RUN_TEST_CMD=generate
  WHAT_WE_ARE_DOING="Generating"

elif [ "$1" == "$BUILD_MODE_GENERATE_MAIN" ]; then

  OUTPUT_DIR=$2

  if [[ "${OUTPUT_DIR:0:1}" != "/" ]] ; then
    OUTPUT_DIR=$PWD/$OUTPUT_DIR
  fi

  if [ -z $OUTPUT_DIR ]; then
    echoIfError "Output directory parameter should not be empty"
    echoIfError "Generate mode command format: tests-all.sh generate <output-dir>"
    exit 1
  fi

  createDir "Tests output" $OUTPUT_DIR

  RUN_TEST_CMD=generate_main
  WHAT_WE_ARE_DOING="Generating $MAIN_SRC files for"

else
  RUN_TEST_CMD=execute
  WHAT_WE_ARE_DOING="Testing"
fi


for d in * ; do
    if [ ! -d "$d" ]; then
        continue
    fi

    if [[ $d = .* ]]; then
        continue
    fi

    echoIfSilent "$WHAT_WE_ARE_DOING '$d'"
    pushd $d > /dev/null
    $RUN_TEST_CMD $d
    printResult $? $d
    popd > /dev/null
    echoIfSilent
done

if [ "$1" == "$BUILD_MODE_GENERATE" ]; then
  copyFile common.sh $OUTPUT_DIR
  copyFile test-all.sh $OUTPUT_DIR
  copyFile preamble.hpp $OUTPUT_DIR
fi