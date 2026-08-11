cmake_minimum_required(VERSION 3.20)
project(KonFanLaLiao LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED NX_ROOT AND NOT "${NX_ROOT}" STREQUAL "")
    set(NX_INSTALL_ROOT "${NX_ROOT}")
elseif(DEFINED ENV{UGII_BASE_DIR})
    set(NX_INSTALL_ROOT "$ENV{UGII_BASE_DIR}")
else()
    message(FATAL_ERROR "Set NX_ROOT or UGII_BASE_DIR to the NX installation root")
endif()

add_library(KonFanLaLiao SHARED
    KonFanLaLiao.cpp
    KonFanLaLiaoMain.cpp)

add_library(KonFanLaLiaoCore SHARED
    KonFanLaLiaoCore.cpp)
set_target_properties(KonFanLaLiaoCore PROPERTIES
    OUTPUT_NAME "KonFanLaLiaoCoreFix4")

target_include_directories(KonFanLaLiao PRIVATE
    "${NX_INSTALL_ROOT}/UGOPEN"
    "${NX_INSTALL_ROOT}/UGOPEN/NXOpen"
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(KonFanLaLiao PRIVATE
    UNICODE _UNICODE NOMINMAX _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Release>:ZH_PROTECTED_BUILD=1>
    $<$<CONFIG:Release>:NDEBUG>)
target_compile_options(KonFanLaLiao PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc /utf-8>)
target_link_directories(KonFanLaLiao PRIVATE
    "${NX_INSTALL_ROOT}/UGOPEN")
target_link_options(KonFanLaLiao PRIVATE
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/RELEASE>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/DYNAMICBASE>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/NXCOMPAT>)
target_link_libraries(KonFanLaLiao PRIVATE
    libugopenint.lib
    libufun.lib
    libnxopencpp.lib
    libnxopencpp_gateway.lib
    libnxopencpp_features.lib
    libnxopenuicpp.lib
    user32.lib
    shell32.lib)

target_include_directories(KonFanLaLiaoCore PRIVATE
    "${NX_INSTALL_ROOT}/UGOPEN"
    "${NX_INSTALL_ROOT}/UGOPEN/NXOpen"
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(KonFanLaLiaoCore PRIVATE
    UNICODE _UNICODE NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_compile_options(KonFanLaLiaoCore PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc /utf-8>)
target_link_directories(KonFanLaLiaoCore PRIVATE
    "${NX_INSTALL_ROOT}/UGOPEN")
target_link_libraries(KonFanLaLiaoCore PRIVATE
    libugopenint.lib
    libufun.lib
    libnxopencpp.lib
    libnxopencpp_gateway.lib
    libnxopencpp_features.lib)

add_custom_command(TARGET KonFanLaLiao POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/KonFanLaLiao.dlx"
        "$<TARGET_FILE_DIR:KonFanLaLiao>/KonFanLaLiao.dlx"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/KonFanLaLiao.bmp"
        "$<TARGET_FILE_DIR:KonFanLaLiao>/KonFanLaLiao.bmp"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/CustomFeatureConfiguration.xml"
        "$<TARGET_FILE_DIR:KonFanLaLiao>/CustomFeatureConfiguration.xml")
