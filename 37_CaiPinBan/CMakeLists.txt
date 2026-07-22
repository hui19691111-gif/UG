cmake_minimum_required(VERSION 3.20)
project(CaiPinBan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT DEFINED ENV{UGII_BASE_DIR})
    message(FATAL_ERROR "UGII_BASE_DIR is not set")
endif()

add_library(CaiPinBan SHARED CaiPinBan.cpp CaiPinBanMain.cpp)
target_include_directories(CaiPinBan PRIVATE "$ENV{UGII_BASE_DIR}/UGOPEN")
target_compile_definitions(CaiPinBan PRIVATE UNICODE _UNICODE NOMINMAX)
target_compile_options(CaiPinBan PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc /utf-8>)
target_link_directories(CaiPinBan PRIVATE "$ENV{UGII_BASE_DIR}/UGOPEN")
target_link_libraries(CaiPinBan PRIVATE
    libugopenint.lib libufun.lib libnxopencpp.lib
    libnxopencpp_gateway.lib libnxopencpp_features.lib
    libnxopencpp_geometricutilities.lib libnxopenuicpp.lib)
add_custom_command(TARGET CaiPinBan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/CaiPinBan.dlx"
        "$<TARGET_FILE_DIR:CaiPinBan>/CaiPinBan.dlx")
