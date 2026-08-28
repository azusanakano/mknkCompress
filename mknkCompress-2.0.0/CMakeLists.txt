cmake_minimum_required(VERSION 3.24)
project(mknkCompressNative VERSION 2.0.0 LANGUAGES CXX RC)

add_executable(mknkCompress WIN32
  src/main.cpp
  resources/mknkCompress.rc
  resources/app.manifest
)
target_compile_features(mknkCompress PRIVATE cxx_std_20)
target_compile_definitions(mknkCompress PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(mknkCompress PRIVATE /W4 /permissive- /utf-8 /EHsc /guard:cf)
target_link_options(mknkCompress PRIVATE /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA)
target_link_libraries(mknkCompress PRIVATE
  windowscodecs ole32 oleaut32 uuid shell32 shlwapi comctl32 comdlg32 uxtheme dwmapi
)
set_target_properties(mknkCompress PROPERTIES
  OUTPUT_NAME "mknkCompress"
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

install(TARGETS mknkCompress RUNTIME DESTINATION .)
install(FILES README.md LICENSE DESTINATION .)
