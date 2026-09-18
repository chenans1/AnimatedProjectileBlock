# CMakeList.txt : CMake project for newTemplate, include source and define
# project specific logic here.
cmake_minimum_required (VERSION 3.19)

# Set your project name. This will be the name of your SKSE .dll file.
project(zAnimatedProjectileBlock VERSION 0.0.1 LANGUAGES CXX)

set(OUTPUT_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/output")

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)

# Dependencies
if(NOT DEFINED COMMONLIBSSE_PATH)
    message(FATAL_ERROR "COMMONLIBSSE_PATH is not defined")
endif()

add_subdirectory(
    "${COMMONLIBSSE_PATH}"
    "${CMAKE_BINARY_DIR}/CommonLibSSE"
)

include("${COMMONLIBSSE_PATH}/cmake/CommonLibSSE.cmake")

# Plugin Sources
set(SOURCES
    src/PCH.h
    src/plugin.cpp
    src/hooks.h
    src/settings.h
    src/settings.cpp
)

add_commonlibsse_plugin(
    ${PROJECT_NAME}
    SOURCES ${SOURCES}
)

target_compile_features(
    ${PROJECT_NAME}
    PRIVATE cxx_std_23
)

target_precompile_headers(
    ${PROJECT_NAME}
    PRIVATE src/PCH.h
)

#extern stuff for skse menuframework/csimpleini etc. 
if(DEFINED SKSE_EXTERN_PATH)
    target_include_directories(
        ${PROJECT_NAME}
        PRIVATE
            "${SKSE_EXTERN_PATH}"
    )
endif()

# When your SKSE .dll is compiled, this will automatically copy the .dll into your mods folder.
# Only works if you configure DEPLOY_ROOT above (or set the SKYRIM_MODS_FOLDER environment variable)
if(DEFINED OUTPUT_FOLDER)
    # If you specify an <OUTPUT_FOLDER> (including via environment variables)
    # then we'll copy your mod files into Skyrim or a mod manager for you!

    # Copy the SKSE plugin .dll files into the SKSE/Plugins/ folder
    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")

    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )
    add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
            VERBATIM
        )
   
endif()
