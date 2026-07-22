cmake_minimum_required(VERSION 3.20)
project(CaiRBan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT DEFINED ENV{UGII_BASE_DIR})
    message(FATAL_ERROR "UGII_BASE_DIR is not set")
endif()

add_library(CaiRBan SHARED CaiRBan.cpp CaiRBanMain.cpp)
target_include_directories(CaiRBan PRIVATE "$ENV{UGII_BASE_DIR}/UGOPEN")
target_compile_definitions(CaiRBan PRIVATE UNICODE _UNICODE NOMINMAX)
target_compile_options(CaiRBan PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc /utf-8>)
target_link_directories(CaiRBan PRIVATE "$ENV{UGII_BASE_DIR}/UGOPEN")
target_link_libraries(CaiRBan PRIVATE
    libugopenint.lib libufun.lib libnxopencpp.lib
    libnxopencpp_gateway.lib libnxopencpp_features.lib
    libnxopencpp_geometricutilities.lib libnxopenuicpp.lib)
add_custom_command(TARGET CaiRBan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/CaiRBan.dlx"
        "$<TARGET_FILE_DIR:CaiRBan>/CaiRBan.dlx"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/CaiRBan.bmp"
        "$<TARGET_FILE_DIR:CaiRBan>/CaiRBan.bmp"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/CustomFeatureConfiguration.xml"
        "$<TARGET_FILE_DIR:CaiRBan>/CustomFeatureConfiguration.xml")
