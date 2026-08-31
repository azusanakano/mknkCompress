cmake_minimum_required(VERSION 3.24)
project(mknkYahooCompressNative VERSION 3.0.0 LANGUAGES CXX RC)

add_executable(mknkYahooCompress WIN32
  src/main.cpp
  resources/mknkCompress.rc
  resources/app.manifest
)
target_compile_features(mknkYahooCompress PRIVATE cxx_std_20)
target_compile_definitions(mknkYahooCompress PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(mknkYahooCompress PRIVATE /W4 /permissive- /utf-8 /EHsc /guard:cf)
target_link_options(mknkYahooCompress PRIVATE /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA)
target_link_libraries(mknkYahooCompress PRIVATE
  windowscodecs ole32 shell32 shlwapi comctl32 comdlg32 uxtheme dwmapi
)
set_target_properties(mknkYahooCompress PROPERTIES
  OUTPUT_NAME "mknkYahooCompress"
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

install(TARGETS mknkYahooCompress RUNTIME DESTINATION .)
install(FILES README.md LICENSE DESTINATION .)
