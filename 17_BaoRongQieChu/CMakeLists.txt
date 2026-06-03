cmake_minimum_required(VERSION 3.20)

project(BaoRongQieChu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED ENV{UGII_BASE_DIR})
    message(FATAL_ERROR "Please set UGII_BASE_DIR to your NX installation root, for example D:/Program Files/Siemens/NX2412.")
endif()

set(NX_ROOT "$ENV{UGII_BASE_DIR}")
set(NX_OPEN_INCLUDE_DIR "${NX_ROOT}/UGOPEN")
set(NX_OPEN_LIBRARY_DIR "${NX_ROOT}/UGOPEN")

add_library(BaoRongQieChu SHARED
    BaoRongQieChu.cpp
    BaoRongQieChuMain.cpp
)

target_include_directories(BaoRongQieChu
    PRIVATE
        ${NX_OPEN_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(BaoRongQieChu PRIVATE UNICODE _UNICODE NOMINMAX)

target_compile_options(BaoRongQieChu PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /EHsc>
)

target_link_directories(BaoRongQieChu PRIVATE ${NX_OPEN_LIBRARY_DIR})

target_link_libraries(BaoRongQieChu PRIVATE
    libugopenint.lib
    libnxopencpp.lib
    libnxopencpp_features.lib
    libnxopencpp_geometricutilities.lib
    libnxopencpp_gateway.lib
    libnxopencpp_modlutils.lib
    libnxopenuicpp.lib
    libufun.lib
)

set_target_properties(BaoRongQieChu PROPERTIES
    OUTPUT_NAME "BaoRongQieChu"
)

add_custom_command(TARGET BaoRongQieChu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/BaoRongQieChu.dlx"
        "$<TARGET_FILE_DIR:BaoRongQieChu>/BaoRongQieChu.dlx"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:BaoRongQieChu>/ui"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/BaoRongQieChu.dlx"
        "$<TARGET_FILE_DIR:BaoRongQieChu>/ui/BaoRongQieChu.dlx"
)
