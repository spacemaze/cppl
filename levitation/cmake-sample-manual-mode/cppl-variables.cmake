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
