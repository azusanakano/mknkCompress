cmake_minimum_required(VERSION 3.24)
project(mknkYahooCompressNative VERSION 3.0.4 LANGUAGES CXX RC)

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

option(MKNK_BUILD_WINDOW_TESTS "Build Win32 window-placement integration tests" OFF)
option(MKNK_BUILD_COMPRESSION_TESTS "Build WIC compression-queue and GUI integration tests" OFF)
if(MKNK_BUILD_WINDOW_TESTS OR MKNK_BUILD_COMPRESSION_TESTS)
  enable_testing()
endif()
if(MKNK_BUILD_WINDOW_TESTS)
  add_executable(window-placement-tests tests/window-placement.cpp)
  target_compile_features(window-placement-tests PRIVATE cxx_std_20)
  target_compile_definitions(window-placement-tests PRIVATE UNICODE _UNICODE NOMINMAX _WIN32_WINNT=0x0A00)
  target_compile_options(window-placement-tests PRIVATE /W4 /utf-8)
  target_link_libraries(window-placement-tests PRIVATE user32)
  set_target_properties(window-placement-tests PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_test(NAME window-placement COMMAND window-placement-tests)
endif()

if(MKNK_BUILD_COMPRESSION_TESTS)
  foreach(scenario IN ITEMS compression-queue compression-ui)
    add_executable(${scenario}-tests tests/${scenario}.cpp)
    target_compile_features(${scenario}-tests PRIVATE cxx_std_20)
    target_compile_definitions(${scenario}-tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
    target_compile_options(${scenario}-tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    target_link_libraries(${scenario}-tests PRIVATE
      windowscodecs ole32 oleaut32 uuid shell32 shlwapi comctl32 comdlg32 uxtheme dwmapi gdi32
    )
    set_target_properties(${scenario}-tests PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_test(NAME ${scenario} COMMAND ${scenario}-tests)
  endforeach()
endif()
