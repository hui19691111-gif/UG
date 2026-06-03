cmake_minimum_required(VERSION 3.20)

project(TwoPointBiLanCao LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED ENV{UGII_BASE_DIR})
    message(FATAL_ERROR "Please set UGII_BASE_DIR to your NX installation root, for example D:/Program Files/Siemens/NX2412.")
endif()

set(NX_ROOT "$ENV{UGII_BASE_DIR}")
set(NX_OPEN_INCLUDE_DIR "${NX_ROOT}/UGOPEN")
set(NX_OPEN_LIBRARY_DIR "${NX_ROOT}/UGOPEN")

add_library(TwoPointBiLanCao SHARED
    TwoPointBiLanCao.cpp
    TwoPointBiLanCaoMain.cpp
)

target_include_directories(TwoPointBiLanCao
    PRIVATE
        ${NX_OPEN_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(TwoPointBiLanCao PRIVATE UNICODE _UNICODE NOMINMAX)

target_compile_options(TwoPointBiLanCao PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc>
)

target_link_directories(TwoPointBiLanCao PRIVATE ${NX_OPEN_LIBRARY_DIR})

target_link_libraries(TwoPointBiLanCao PRIVATE
    libugopenint.lib
    libnxopencpp.lib
    libnxopencpp_gateway.lib
    libnxopenuicpp.lib
    libufun.lib
)

set_target_properties(TwoPointBiLanCao PROPERTIES
    OUTPUT_NAME "TwoPointBiLanCao"
)

add_custom_command(TARGET TwoPointBiLanCao POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/TwoPointBiLanCao.dlx"
        "$<TARGET_FILE_DIR:TwoPointBiLanCao>/TwoPointBiLanCao.dlx"
)
