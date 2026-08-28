cmake_minimum_required(VERSION 3.20)
project(mknkCompressLinux VERSION 3.0.0 LANGUAGES CXX)

add_executable(mknkCompress src/main.cpp)
target_compile_features(mknkCompress PRIVATE cxx_std_20)
target_compile_options(mknkCompress PRIVATE -Wall -Wextra -Wpedantic -O2)
target_link_options(mknkCompress PRIVATE -static-libstdc++ -static-libgcc -Wl,--as-needed)
target_link_libraries(mknkCompress PRIVATE
  -l:libgtk-3.so.0
  -l:libgdk-3.so.0
  -l:libgobject-2.0.so.0
  -l:libglib-2.0.so.0
  pthread
)

install(TARGETS mknkCompress RUNTIME DESTINATION .)
install(DIRECTORY app DESTINATION .)
install(FILES README.md LICENSE THIRD-PARTY-NOTICES.md DESTINATION .)
