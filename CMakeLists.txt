 cmake_minimum_required(VERSION 3.20)
 project(blackhole-standalone LANGUAGES CXX)
 
 set(CMAKE_CXX_STANDARD 17)
 set(CMAKE_CXX_STANDARD_REQUIRED ON)
 
 # MSYS2 UCRT64 prefix for GLFW
 list(APPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")
 
 find_package(glfw3 REQUIRED)
 
 # OpenGL
 find_package(OpenGL REQUIRED)
 
 add_executable(blackhole
     src/main.cpp
 )
 
 target_include_directories(blackhole PRIVATE
     ${OPENGL_INCLUDE_DIRS}
 )
 
 target_link_libraries(blackhole PRIVATE
     glfw::glfw
     ${OPENGL_LIBRARIES}
 )
 
 # Compile with MSYS2 GCC (ucrt64)
 if(MINGW)
     target_compile_options(blackhole PRIVATE -mwindows)
 endif()
