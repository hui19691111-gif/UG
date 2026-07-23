cmake_minimum_required(VERSION 3.20)
project(CaiR1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT DEFINED ENV{UGII_BASE_DIR})
    message(FATAL_ERROR "UGII_BASE_DIR is not set")
endif()

add_library(CaiR1 SHARED CaiR1.cpp CaiR1Main.cpp)
add_library(CaiR1Core SHARED CaiR1Core.cpp)
set_target_properties(CaiR1Core PROPERTIES
    OUTPUT_NAME CaiR1CoreEdit)
target_include_directories(CaiR1 PRIVATE
    "$ENV{UGII_BASE_DIR}/UGOPEN"
    "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_compile_definitions(CaiR1 PRIVATE
    ZH_PROTECTED_BUILD=1 UNICODE _UNICODE NOMINMAX)
target_compile_options(CaiR1 PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc /utf-8>)
target_link_directories(CaiR1 PRIVATE "$ENV{UGII_BASE_DIR}/UGOPEN")
target_link_libraries(CaiR1 PRIVATE
    libugopenint.lib libufun.lib libnxopencpp.lib
    libnxopencpp_gateway.lib libnxopencpp_features.lib
    libnxopencpp_geometricutilities.lib libnxopenuicpp.lib)
target_include_directories(CaiR1Core PRIVATE
    "$ENV{UGII_BASE_DIR}/UGOPEN")
target_compile_definitions(CaiR1Core PRIVATE
    ZH_PROTECTED_BUILD=1 UNICODE _UNICODE NOMINMAX)
target_compile_options(CaiR1Core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc /utf-8>)
target_link_directories(CaiR1Core PRIVATE "$ENV{UGII_BASE_DIR}/UGOPEN")
target_link_libraries(CaiR1Core PRIVATE
    libugopenint.lib libufun.lib libnxopencpp.lib
    libnxopencpp_gateway.lib libnxopencpp_features.lib)
add_custom_command(TARGET CaiR1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/CaiR1.dlx"
        "$<TARGET_FILE_DIR:CaiR1>/CaiR1.dlx"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/CaiR1.bmp"
        "$<TARGET_FILE_DIR:CaiR1>/CaiR1.bmp"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/CustomFeatureConfiguration.xml"
        "$<TARGET_FILE_DIR:CaiR1>/CustomFeatureConfiguration.xml")
