cmake_minimum_required(VERSION 3.20)

project(BiaoJIXIan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED ENV{UGII_BASE_DIR})
    message(FATAL_ERROR "Please set UGII_BASE_DIR to your NX installation root, for example D:/Program Files/Siemens/NX2412.")
endif()

set(NX_ROOT "$ENV{UGII_BASE_DIR}")
set(NX_OPEN_INCLUDE_DIR "${NX_ROOT}/UGOPEN")
set(NX_OPEN_LIBRARY_DIR "${NX_ROOT}/UGOPEN")

add_library(BiaoJIXIan SHARED
    BiaoJiXian_BJ.cpp
    MarkLineBuilder.cpp
)

target_include_directories(BiaoJIXIan
    PRIVATE
        ${NX_OPEN_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(BiaoJIXIan PRIVATE UNICODE _UNICODE NOMINMAX)

target_compile_options(BiaoJIXIan PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc>
)

target_link_directories(BiaoJIXIan PRIVATE ${NX_OPEN_LIBRARY_DIR})

target_link_libraries(BiaoJIXIan PRIVATE
    libugopenint.lib
    libnxopencpp.lib
    libnxopencpp_features.lib
    libnxopencpp_geometricutilities.lib
    libnxopencpp_gateway.lib
    libnxopencpp_modlutils.lib
    libnxopencpp_drawings.lib
    libnxopencpp_drafting.lib
    libnxopencpp_preferences.lib
    libnxopencpp_sheetmetal.lib
    libnxopenuicpp.lib
    libufun.lib
)

set_target_properties(BiaoJIXIan PROPERTIES
    OUTPUT_NAME "BiaoJIXIan"
)

add_custom_command(TARGET BiaoJIXIan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:BiaoJIXIan>/ui"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/BiaoJIXIan.dlx"
        "$<TARGET_FILE_DIR:BiaoJIXIan>/BiaoJIXIan.dlx"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/BiaoJIXIan.dlx"
        "$<TARGET_FILE_DIR:BiaoJIXIan>/ui/BiaoJIXIan.dlx"
)
