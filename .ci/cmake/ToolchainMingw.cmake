set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_AR           x86_64-w64-mingw32-ar CACHE FILEPATH "" FORCE)

set(CMAKE_C_LINK_EXECUTABLE x86_64-w64-mingw32-ld)
set(CMAKE_CXX_LINK_EXECUTABLE x86_64-w64-mingw32-ld)

set(CMAKE_LINKER       x86_64-w64-mingw32-ld CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB       x86_64-w64-mingw32-ranlib)
set(CMAKE_STRIP        x86_64-w64-mingw32-strip)

