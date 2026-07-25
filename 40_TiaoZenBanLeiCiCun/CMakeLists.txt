cmake_minimum_required(VERSION 3.20)
project(TiaoZenBanLeiCiCun LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ENV{UGII_BASE_DIR})
    message(FATAL_ERROR "UGII_BASE_DIR is not set")
endif()

add_library(TiaoZenBanLeiCiCun SHARED
    TiaoZenBanLeiCiCun.cpp
    TiaoZenBanLeiCiCunMain.cpp)

target_include_directories(TiaoZenBanLeiCiCun PRIVATE
    "$ENV{UGII_BASE_DIR}/UGOPEN"
    "$ENV{UGII_BASE_DIR}/UGOPEN/NXOpen"
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(TiaoZenBanLeiCiCun PRIVATE
    ZH_PROTECTED_BUILD=1
    UNICODE _UNICODE NOMINMAX
    _CRT_SECURE_NO_WARNINGS)
target_compile_options(TiaoZenBanLeiCiCun PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W3 /EHsc /utf-8>)
target_link_directories(TiaoZenBanLeiCiCun PRIVATE
    "$ENV{UGII_BASE_DIR}/UGOPEN")
target_link_libraries(TiaoZenBanLeiCiCun PRIVATE
    libugopenint.lib
    libufun.lib
    libnxopencpp.lib
    libnxopencpp_gateway.lib
    libnxopencpp_features.lib
    libnxopencpp_geometricutilities.lib
    libnxopenuicpp.lib
    user32.lib)

add_custom_command(TARGET TiaoZenBanLeiCiCun POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/TiaoZenBanLeiCiCun.dlx"
        "$<TARGET_FILE_DIR:TiaoZenBanLeiCiCun>/TiaoZenBanLeiCiCun.dlx"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/TiaoZenBanLeiCiCun.bmp"
        "$<TARGET_FILE_DIR:TiaoZenBanLeiCiCun>/TiaoZenBanLeiCiCun.bmp"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/TiaoZenBanLeiCiCunGuide.bmp"
        "$<TARGET_FILE_DIR:TiaoZenBanLeiCiCun>/TiaoZenBanLeiCiCunGuide.bmp")
