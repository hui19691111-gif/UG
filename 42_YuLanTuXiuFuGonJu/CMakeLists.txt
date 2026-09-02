cmake_minimum_required(VERSION 3.20)
project(YuLanTuXiuFuGonJu LANGUAGES CXX RC)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(DEFINED NX_ROOT AND NOT "${NX_ROOT}" STREQUAL "")
  set(UGII_SDK_ROOT "${NX_ROOT}")
elseif(DEFINED ENV{UGII_BASE_DIR})
  set(UGII_SDK_ROOT "$ENV{UGII_BASE_DIR}")
else()
  message(FATAL_ERROR "NX_ROOT and UGII_BASE_DIR are not set")
endif()
add_library(YuLanTuXiuFuGonJu SHARED YuLanTuXiuFuGonJu.cpp)
add_library(NxPartThumbnailProvider SHARED NxPartThumbnailProvider.cpp NxPartThumbnailProvider.def)
add_executable(NxPreviewRepair WIN32 NxPreviewRepair.cpp NxPreviewRepair.rc)
target_include_directories(YuLanTuXiuFuGonJu PRIVATE
  "${UGII_SDK_ROOT}/UGOPEN" "${UGII_SDK_ROOT}/UGOPEN/NXOpen")
target_link_directories(YuLanTuXiuFuGonJu PRIVATE "${UGII_SDK_ROOT}/UGOPEN")
target_compile_definitions(YuLanTuXiuFuGonJu PRIVATE
  UNICODE _UNICODE NOMINMAX _CRT_SECURE_NO_WARNINGS
  $<$<CONFIG:Release>:ZH_PROTECTED_BUILD=1 NDEBUG>)
target_compile_options(YuLanTuXiuFuGonJu PRIVATE /W4 /EHsc /utf-8)
target_link_options(YuLanTuXiuFuGonJu PRIVATE
  $<$<CONFIG:Release>:/RELEASE /DYNAMICBASE /NXCOMPAT>)
target_link_libraries(YuLanTuXiuFuGonJu PRIVATE
  libnxopencpp.lib libnxopencpp_gateway.lib libnxopenuicpp.lib
  libugopenint.lib libufun.lib user32.lib shell32.lib ole32.lib)
target_compile_definitions(NxPartThumbnailProvider PRIVATE UNICODE _UNICODE NOMINMAX NDEBUG)
target_compile_options(NxPartThumbnailProvider PRIVATE /W4 /EHsc /utf-8)
target_link_options(NxPartThumbnailProvider PRIVATE /RELEASE /DYNAMICBASE /NXCOMPAT)
target_link_libraries(NxPartThumbnailProvider PRIVATE windowscodecs.lib shlwapi.lib ole32.lib)
target_compile_definitions(NxPreviewRepair PRIVATE UNICODE _UNICODE NOMINMAX NDEBUG)
target_compile_options(NxPreviewRepair PRIVATE /W4 /EHsc /utf-8)
target_link_options(NxPreviewRepair PRIVATE /RELEASE /DYNAMICBASE /NXCOMPAT)
target_link_libraries(NxPreviewRepair PRIVATE comctl32.lib shell32.lib advapi32.lib)
