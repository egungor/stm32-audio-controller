# Cross-build the Windows host program and its tests on Linux with
# MinGW-w64 (Debian/Ubuntu package: g++-mingw-w64-x86-64-posix).
#
#   cmake -S host -B build/host-mingw -DCMAKE_TOOLCHAIN_FILE=$PWD/host/cmake/mingw-w64.cmake
#   cmake --build build/host-mingw
#
# The "-posix" compilers use the POSIX thread model, which std::thread
# (EventQueue, WinHidLink) needs. Not the reference build: Windows CI
# builds with MSVC. See host/README.md, "Build and test".
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
