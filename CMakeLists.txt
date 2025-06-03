cmake_minimum_required(VERSION 3.16)
project(CasioEmuX LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all source files in emulator/ and subdirectories
file(GLOB_RECURSE EMULATOR_SOURCES emulator/*.cpp)

add_executable(CasioEmuX ${EMULATOR_SOURCES})

# SDL2
find_package(SDL2 REQUIRED)
target_include_directories(CasioEmuX PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(CasioEmuX PRIVATE ${SDL2_LIBRARIES})

# SDL2_image
find_package(SDL2_image REQUIRED)
target_include_directories(CasioEmuX PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
target_link_libraries(CasioEmuX PRIVATE ${SDL2_IMAGE_LIBRARIES})

# Lua 5.3
find_package(Lua 5.3 REQUIRED)
target_include_directories(CasioEmuX PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(CasioEmuX PRIVATE ${LUA_LIBRARIES})

# Python 3.10 (optional, remove if not needed)
find_package(Python3 3.10 COMPONENTS Interpreter Development REQUIRED)
target_include_directories(CasioEmuX PRIVATE ${Python3_INCLUDE_DIRS})
target_link_libraries(CasioEmuX PRIVATE ${Python3_LIBRARIES})

# ImGui using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(IMGUI REQUIRED IMPORTED_TARGET imgui)
target_link_libraries(CasioEmuX PRIVATE PkgConfig::IMGUI SDL2_image SDL2)

# Set runtime directory
set_target_properties(CasioEmuX PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")

option(ENABLE_ASAN "Enable AddressSanitizer" ON)

if(ENABLE_ASAN)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(CasioEmuX PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(CasioEmuX PRIVATE -fsanitize=address)
  endif()
endif()

# Disassembler executables
add_executable(CasioDisasMain disas/main.cpp disas/lib.cpp)
# add_executable(CasioDisasTemp disas/temp.cpp disas/lib.cpp)
# Optionally link libraries if needed (uncomment and adjust as required)
# target_link_libraries(CasioDisasMain PRIVATE ...)
# target_link_libraries(CasioDisasTemp PRIVATE ...)