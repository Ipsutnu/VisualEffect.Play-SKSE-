cmake_minimum_required(VERSION 3.21)

project(EffectListener VERSION 1.2.0 LANGUAGES CXX)

# ============================================================
# CommonLibSSE-NG
# ============================================================

add_subdirectory(lib/commonlibsse-ng)

include(lib/commonlibsse-ng/cmake/CommonLibSSE.cmake)


# ============================================================
# Output
# ============================================================

if(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/Data")
endif()

if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/${PROJECT_NAME}")
endif()


# ============================================================
# ImGui
# ============================================================

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp

    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)


# ============================================================
# MinHook
# ============================================================

set(MINHOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/minhook")

set(MINHOOK_SOURCES
    "${MINHOOK_DIR}/src/buffer.c"
    "${MINHOOK_DIR}/src/hook.c"
    "${MINHOOK_DIR}/src/trampoline.c"
    "${MINHOOK_DIR}/src/hde/hde64.c"
)

set_source_files_properties(
    ${MINHOOK_SOURCES}
    PROPERTIES
    LANGUAGE C
    SKIP_PRECOMPILE_HEADERS ON
)


# ============================================================
# SKSE Plugin
# ============================================================

add_commonlibsse_plugin(${PROJECT_NAME}
    SOURCES
        src/plugin.cpp
        src/Logger.cpp
        src/EffectControl.cpp
        src/EffectDatabase.cpp

        ${IMGUI_SOURCES}
        ${MINHOOK_SOURCES}
)


# ============================================================
# C++ Standard
# ============================================================

target_compile_features(${PROJECT_NAME} PRIVATE
    cxx_std_23
)


# ============================================================
# Precompiled Header
# ============================================================

target_precompile_headers(${PROJECT_NAME} PRIVATE
    src/PCH.h
)


# ============================================================
# Include Directories
# ============================================================

target_include_directories(${PROJECT_NAME} PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${MINHOOK_DIR}/include
)


# ============================================================
# CommonLibSSE-NG
# ============================================================

target_link_libraries(${PROJECT_NAME} PRIVATE
    CommonLibSSE::CommonLibSSE

    d3d11
    dxgi
)


# ============================================================
# Definitions
# ============================================================

target_compile_definitions(${PROJECT_NAME} PRIVATE
    IMGUI_DEFINE_MATH_OPERATORS
)


# ============================================================
# Deploy
# ============================================================

if(DEFINED OUTPUT_FOLDER)

    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")

    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD

        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${DLL_FOLDER}"

        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${PROJECT_NAME}>"
            "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"

        VERBATIM
    )

    # Copy PDB on Debug
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")

        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD

            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_PDB_FILE:${PROJECT_NAME}>"
                "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"

            VERBATIM
        )

    endif()

endif()