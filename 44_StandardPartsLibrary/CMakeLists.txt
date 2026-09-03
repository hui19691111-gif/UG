cmake_minimum_required(VERSION 3.20)
project(StandardPartsLibrary LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED NX_ROOT AND NOT "${NX_ROOT}" STREQUAL "")
    set(NX_INSTALL_ROOT "${NX_ROOT}")
elseif(DEFINED ENV{UGII_BASE_DIR})
    set(NX_INSTALL_ROOT "$ENV{UGII_BASE_DIR}")
else()
    message(FATAL_ERROR "Set NX_ROOT or UGII_BASE_DIR to the NX installation root")
endif()

add_library(StandardPartsLibrary SHARED
    StandardPartsLibrary.cpp
    StandardPartsLibraryMain.cpp)

target_include_directories(StandardPartsLibrary PRIVATE
    "${NX_INSTALL_ROOT}/UGOPEN"
    "${NX_INSTALL_ROOT}/UGOPEN/NXOpen"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../common")
target_compile_definitions(StandardPartsLibrary PRIVATE
    UNICODE _UNICODE NOMINMAX _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Release>:ZH_PROTECTED_BUILD=1>
    $<$<CONFIG:Release>:NDEBUG>)
target_compile_options(StandardPartsLibrary PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc /utf-8>)
target_link_directories(StandardPartsLibrary PRIVATE "${NX_INSTALL_ROOT}/UGOPEN")
target_link_options(StandardPartsLibrary PRIVATE
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/RELEASE>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/DYNAMICBASE>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/NXCOMPAT>)
target_link_libraries(StandardPartsLibrary PRIVATE
    libugopenint.lib libufun.lib user32.lib comctl32.lib comdlg32.lib shell32.lib ole32.lib)

add_custom_command(TARGET StandardPartsLibrary POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/StandardPartsLibrary.bmp"
        "$<TARGET_FILE_DIR:StandardPartsLibrary>/StandardPartsLibrary.bmp")
