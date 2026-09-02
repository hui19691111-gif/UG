cmake_minimum_required(VERSION 3.20)
project(BendMarkNotch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED NX_ROOT AND NOT "${NX_ROOT}" STREQUAL "")
    set(NX_INSTALL_ROOT "${NX_ROOT}")
elseif(DEFINED ENV{UGII_BASE_DIR})
    set(NX_INSTALL_ROOT "$ENV{UGII_BASE_DIR}")
else()
    message(FATAL_ERROR "Set NX_ROOT or UGII_BASE_DIR to the NX installation root")
endif()

add_library(BendMarkNotch SHARED BendMarkNotch.cpp BendMarkNotchMain.cpp)
add_library(BendMarkNotchCore SHARED BendMarkNotchCore.cpp)
add_dependencies(BendMarkNotch BendMarkNotchCore)

target_include_directories(BendMarkNotch PRIVATE
    "${NX_INSTALL_ROOT}/UGOPEN"
    "${NX_INSTALL_ROOT}/UGOPEN/NXOpen"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../common")
target_compile_definitions(BendMarkNotch PRIVATE
    UNICODE _UNICODE NOMINMAX _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Release>:ZH_PROTECTED_BUILD=1>
    $<$<CONFIG:Release>:NDEBUG>)
target_compile_options(BendMarkNotch PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc /utf-8>)
target_link_directories(BendMarkNotch PRIVATE "${NX_INSTALL_ROOT}/UGOPEN")
target_link_options(BendMarkNotch PRIVATE
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/RELEASE>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/DYNAMICBASE>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/NXCOMPAT>)
target_link_libraries(BendMarkNotch PRIVATE
    libugopenint.lib libufun.lib libnxopencpp.lib
    libnxopencpp_gateway.lib libnxopencpp_features.lib
    libnxopencpp_geometricutilities.lib
    libnxopenuicpp.lib user32.lib)

target_include_directories(BendMarkNotchCore PRIVATE
    "${NX_INSTALL_ROOT}/UGOPEN"
    "${NX_INSTALL_ROOT}/UGOPEN/NXOpen"
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(BendMarkNotchCore PRIVATE
    UNICODE _UNICODE NOMINMAX _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Release>:ZH_PROTECTED_BUILD=1>
    $<$<CONFIG:Release>:NDEBUG>)
target_compile_options(BendMarkNotchCore PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc /utf-8>)
target_link_directories(BendMarkNotchCore PRIVATE "${NX_INSTALL_ROOT}/UGOPEN")
target_link_options(BendMarkNotchCore PRIVATE
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/RELEASE>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/DYNAMICBASE>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/NXCOMPAT>)
target_link_libraries(BendMarkNotchCore PRIVATE
    libugopenint.lib libufun.lib libnxopencpp.lib
    libnxopencpp_gateway.lib libnxopencpp_features.lib)

add_custom_command(TARGET BendMarkNotch POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/BendMarkNotch.dlx"
        "$<TARGET_FILE_DIR:BendMarkNotch>/BendMarkNotch.dlx"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/BendMarkNotch.bmp"
        "$<TARGET_FILE_DIR:BendMarkNotch>/BendMarkNotch.bmp"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/CustomFeatureConfiguration.xml"
        "$<TARGET_FILE_DIR:BendMarkNotch>/CustomFeatureConfiguration.xml")
