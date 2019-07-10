include(../cmake-builer/log.cmake)
include(../cmake-builer/file.cmake)

macro(levitationProject name)
    cmake_policy(SET CMP0054 NEW)
    cmake_policy(SET CMP0043 NEW)
    project(${name})
endmacro()

macro (getDirectDependenciesListFileForDeclAst fileNameVar sourceFile)
    setupOutputFile(${fileNameVar} ${sourceFile}
        "${LEVITATION_DeclarationAST_EXTENSION}.${LEVITATION_DirectDependencies_EXTENSION}"
    )
endmacro()

macro (getDirectDependenciesListFileForObj fileNameVar sourceFile)
    setupOutputFile(${fileNameVar} ${sourceFile}
        "${LEVITATION_Object_EXTENSION}.${LEVITATION_DirectDependencies_EXTENSION}"
    )
endmacro()

macro (getFullDependenciesListFileForDeclAst fileNameVar sourceFile)
    setupOutputFile(${fileNameVar} ${sourceFile}
        "${LEVITATION_DeclarationAST_EXTENSION}.${LEVITATION_FullDependencies_EXTENSION}"
    )
endmacro()

macro (getFullDependenciesListFileForObj fileNameVar sourceFile)
    setupOutputFile(${fileNameVar} ${sourceFile}
        "${LEVITATION_Object_EXTENSION}.${LEVITATION_FullDependencies_EXTENSION}"
    )
endmacro()


macro (getDirectDependenciesForDeclAst outputVar sourceFile)
    getDirectDependenciesListFileForDeclAst(depsFile ${sourceFile})
    if (EXISTS ${depsFile})
        file(STRINGS ${depsFile} ${outputVar})
    endif()
endmacro()

macro (getDirectDependenciesForObj outputVar sourceFile)
    getDirectDependenciesListFileForObj(depsFile ${sourceFile})
    if (EXISTS ${depsFile})
        file(STRINGS ${depsFile} ${outputVar})
    endif()
endmacro()

macro (getFullDependenciesForDeclAst outputVar sourceFile)
    getFullDependenciesListFileForDeclAst(depsFile ${sourceFile})
    if (EXISTS ${depsFile})
        file(STRINGS ${depsFile} ${outputVar})
    endif()
endmacro()

macro (getFullDependenciesForObj outputVar sourceFile)
    getFullDependenciesListFileForObj(depsFile ${sourceFile})
    if (EXISTS ${depsFile})
        file(STRINGS ${depsFile} ${outputVar})
    endif()
endmacro()

macro(auxSourcesRecursive dir sourcesVar)
    auxSources(${dir} ${sourcesVar})
    subDirListRecursive(subDirs ${dir})
    foreach(d ${subDirs})
        auxSources(${d} ${sourcesVar})
    endforeach()

    set(mainFilePath ${CMAKE_SOURCE_DIR}/${LEVITATION_MAIN_FILE})

    list(FIND ${sourcesVar} ${mainFilePath} MainIdx)
    if (NOT(${MainIdx} EQUAL -1))
        # We have special case for main file.
        list(REMOVE_AT ${sourcesVar} ${MainIdx})
        set(LEVITATION_MAIN ${mainFilePath})
        info("Found main file ${LEVITATION_MAIN}")
    else()
        error("Main file ${LEVITATION_MAIN_FILE} not found.")
    endif()

    if (EXISTS ${CMAKE_SOURCE_DIR}/${LEVITATION_PREAMBLE_FILE})
        set(LEVITATION_PREAMBLE ${CMAKE_SOURCE_DIR}/${LEVITATION_PREAMBLE_FILE})
        info("Found preamble ${LEVITATION_PREAMBLE}")
    endif()
endmacro()

macro(auxLevitationSourcesRecursive dir sourcesVar)
    filesListMask(${sourcesVar} ${dir} *.${LEVITATION_SourceCode_EXTENSION})
    subDirListRecursive(subDirs ${dir})
    foreach(d ${subDirs})
        filesListMask(${sourcesVar} ${d} *.${LEVITATION_SourceCode_EXTENSION})
    endforeach()
endmacro()

macro(setupOthersCompileTarget)
    add_library(${LEVITATION_OTHERS_STAGE} ${OTHER_SOURCES})
endmacro()

macro(setupPreambleTarget)
    if (LEVITATION_PREAMBLE)
        setupOutputFile(LEVITATION_PREAMBLE_PCH
            ${LEVITATION_PREAMBLE}
            ${PCH_EXTENSION}
        )

        add_custom_command(
            OUTPUT ${LEVITATION_PREAMBLE_PCH}
            COMMAND ${CLANG} ${LEVITATION_CC_FLAGS}
            -xc++ -levitation-build-preamble
            ${LEVITATION_PREAMBLE}
            -o ${LEVITATION_PREAMBLE_PCH}
        )

        add_custom_target(${LEVITATION_PREAMBLE_STAGE}
            DEPENDS ${LEVITATION_PREAMBLE_PCH}
        )
    endif()
endmacro()

macro (addPreambleIfNeeded command)
    if (LEVITATION_PREAMBLE)
        set(${command} ${${command}}
            -levitation-preamble=${LEVITATION_PREAMBLE_PCH}
        )
    endif()
endmacro()

macro(setupLevitationParseTarget)

    set(LEVITATION_PARSED_AST_FILES)
    set(LEVITATION_PARSED_DEPS_FILES)
        
    foreach(levitationSource ${LEVITATION_SOURCES})
        
        setupOutputFile(levitationParsedAST
            ${levitationSource}
            ${LEVITATION_ParsedAST_EXTENSION}
        )
        set(LEVITATION_PARSED_AST_FILES ${LEVITATION_PARSED_AST_FILES}
            ${levitationParsedAST}
        )

        setupOutputFile(parsedDependenciesFile
            ${levitationSource}
            ${LEVITATION_ParsedDependencies_EXTENSION}
        )
        set(LEVITATION_PARSED_DEPS_FILES ${LEVITATION_PARSED_DEPS_FILES}
            ${parsedDependenciesFile}
        )

        createSubDirFor(${levitationParsedAST})

        set(PARSE_COMMAND
            ${CLANG} ${LEVITATION_PARSE_ARGS}
            -levitation-deps-output-file=${parsedDependenciesFile}
            ${levitationSource}
            -o ${levitationParsedAST}
        )

        addPreambleIfNeeded(PARSE_COMMAND)

        add_custom_command(
            OUTPUT ${levitationParsedAST} ${parsedDependenciesFile}
            DEPENDS ${levitationSource}
            COMMAND ${PARSE_COMMAND}
        )

    endforeach()

    trace("Parse Stage will produce: ${LEVITATION_PARSED_AST_FILES}")

    add_custom_target(${LEVITATION_PARSE_STAGE}
        DEPENDS
        ${LEVITATION_OTHERS_STAGE}
        ${LEVITATION_PARSED_AST_FILES}
        ${LEVITATION_PARSED_DEPS_FILES}
    )

    if (LEVITATION_PREAMBLE)
       add_dependencies(${LEVITATION_PARSE_STAGE} ${LEVITATION_PREAMBLE_STAGE})
    endif()

endmacro()

macro(setupSolveDependenciesTarget)

    set(dependenciesLists)

    foreach (s ${LEVITATION_SOURCES})

        trace("Adding dependencies files for ${s}")

        getDirectDependenciesListFileForDeclAst(ddDeclAst ${s})
        getDirectDependenciesListFileForObj(ddObj ${s})
        getFullDependenciesListFileForDeclAst(fulldDeclAst ${s})
        getFullDependenciesListFileForObj(fulldObj ${s})

        set(dependenciesLists ${dependenciesLists}
            ${ddDeclAst}
            ${ddObj}
            ${fulldDeclAst}
            ${fulldObj}
        )
    endforeach()

    trace("Dependencies list files: ${dependenciesLists}")

    if (${LEVITATION_LOG_LEVEL} EQUAL ${LEVITATION_TRACE_LEVEL})
        set(levitationDepsVerbose --verbose)
    endif()

    add_custom_command(
        OUTPUT ${dependenciesLists}
        DEPENDS ${LEVITATION_PARSED_AST_FILES} ${LEVITATION_PARSED_DEPS_FILES}
        COMMAND ${LEVITATION_DEPS}
            -src-root=${CMAKE_SOURCE_DIR}
            -build-root=${PROJECT_BINARY_DIR}
            -main-file=${LEVITATION_MAIN_FILE}
            ${levitationDepsVerbose}
    )

    add_custom_target(${LEVITATION_SOLVE_DEPENDENCIES_STAGE}
        DEPENDS
        ${LEVITATION_PARSE_STAGE}
        ${dependenciesLists}
    )

endmacro()

# We have two main build targets
# 1. Parsing and dependencies solving
#    It produces dependency files and parsed AST files.
# 2. Compilation. Basically it depends on parsed AST and on dependency
#    files. But linking it Compilation output stage may cause
#    rerunning stage #1.
# We will run stage #1 and stage #2 manually from script.
# And since we run #2 after #1, we assume we have everything we need and at
# stage #1 won't rerun.

macro(setupLevitationInstantiateTarget)

    set(declAstFiles)

    foreach (src ${LEVITATION_SOURCES})
        setupOutputFile(declAst ${src}
            "${LEVITATION_DeclarationAST_EXTENSION}"
        )
        set(declAstFiles ${declAstFiles} ${declAst})

        setupOutputFile(parsedAst ${src}
            "${LEVITATION_ParsedAST_EXTENSION}"
        )

        getDirectDependenciesForDeclAst(ddFile ${src})
        getFullDependenciesForDeclAst(fulldFile ${src})

        set(dependenciesArgs)
        foreach (dep ${fulldFile} )
            set(dependenciesArgs ${dependenciesArgs}
                -levitation-dependency=${dep}
            )
        endforeach()

        set(INSTANTIATE_COMMAND
            ${CLANG} ${LEVITATION_INSTANTIATE_ARGS}
            ${dependenciesArgs}
            ${levitationSource}
            ${parsedAst}
            -o ${declAst}
        )

        addPreambleIfNeeded(INSTANTIATE_COMMAND)

        add_custom_command(
            OUTPUT ${declAst}
            DEPENDS ${ddFile} ${fulldFile}
            COMMAND ${INSTANTIATE_COMMAND}
        )
    endforeach()

    add_custom_target(${LEVITATION_INSTANTIATE_STAGE}
        DEPENDS
        ${declAstFiles}
    )

endmacro()

macro(addCompileCommand objFile inputFile directDepsVar fullDepsVar)
    set(dependenciesArgs)
    foreach (dep ${${fullDepsVar}} )
        set(dependenciesArgs ${dependenciesArgs}
            -levitation-dependency=${dep}
        )
    endforeach()

    set(COMPILE_COMMAND
        ${CLANG} ${LEVITATION_COMPILE_ARGS}
        ${dependenciesArgs}
        ${levitationSource}
        ${inputFile}
        -o ${objFile}
    )

    addPreambleIfNeeded(COMPILE_COMMAND)

    trace("Adding input ${inputFile}, depends on ${${directDepsVar}} ${inputFile}")

    add_custom_command(
        OUTPUT ${objFile}
        DEPENDS ${${directDepsVar}} ${inputFile}
        COMMAND ${COMPILE_COMMAND}
    )
endmacro()

macro (addCompileParsedAst objFilesVar srcFile)

    setupOutputFile(objFile ${srcFile}
        "${LEVITATION_Object_EXTENSION}"
    )
    set(${objFilesVar} ${objFiles} ${objFile})

    setupOutputFile(inputFile ${srcFile}
        "${LEVITATION_ParsedAST_EXTENSION}"
    )

    getDirectDependenciesForObj(directDeps ${srcFile})
    getFullDependenciesForObj(fullDeps ${srcFile})

    addCompileCommand(
        ${objFile}
        ${inputFile}
        directDeps
        fullDeps
    )
endmacro()

macro (addCompileMain objFilesVar)
    setupOutputFile(objFile ${LEVITATION_MAIN}
        "${LEVITATION_Object_EXTENSION}"
    )
    set(${objFilesVar} ${objFiles} ${objFile})

    getDirectDependenciesForObj(directDeps ${LEVITATION_MAIN})
    getFullDependenciesForObj(fullDeps ${LEVITATION_MAIN})

    addCompileCommand(
        ${objFile}
        ${LEVITATION_MAIN} #inputFile
        directDeps
        fullDeps
    )
endmacro()

macro(setupLevitationCompileTarget)
    set(objFiles)

    foreach (src ${LEVITATION_SOURCES})
        addCompileParsedAst(objFiles ${src})
    endforeach()

    addCompileMain(objFiles)

    add_custom_target(${LEVITATION_COMPILE_STAGE}
        DEPENDS
        ${objFiles}
    )

    set(LEVITATION_OBJECTS ${objFiles})
endmacro()

macro(setupLinkExecutableTarget outputName)
    set_source_files_properties(
        ${LEVITATION_OBJECTS}
        PROPERTIES
        EXTERNAL_OBJECT true
        GENERATED true
    )

    add_executable( ${outputName} ${LEVITATION_OBJECTS})
    add_dependencies( ${outputName} ${LEVITATION_COMPILE_STAGE})
    target_link_libraries(${outputName} ${LEVITATION_OTHERS_STAGE})
endmacro()

macro(setupLevitationTool name)

    auxSourcesRecursive(${CMAKE_SOURCE_DIR} OTHER_SOURCES)

    auxLevitationSourcesRecursive(${CMAKE_SOURCE_DIR} LEVITATION_SOURCES)

    setupOthersCompileTarget()
    setupPreambleTarget()
    setupLevitationParseTarget()
    setupSolveDependenciesTarget()

    setupLevitationInstantiateTarget()
    setupLevitationCompileTarget()
    setupLinkExecutableTarget(${name})
endmacro()

macro(getTargetArch destVar)
    if (PLATFORM_LINUX_X86_64)
        set(${destVar} "amd64")
    elseif(PLATFORM_DARWIN_X86_64)
        set(${destVar} "darwin-x86_64")
    elseif(PLATFORM_ARM)
        set(${destVar} "armhf")
    else()
        message( FATAL_ERROR "Undefined platform. Sorry I still need it to be known." )
    endif()
endmacro()
