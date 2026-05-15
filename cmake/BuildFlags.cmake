set(CMAKE_CXX_STANDARD 23)
set(CXX_STANDARD_REQUIRED ON)

message("C++ version: ${CMAKE_CXX_STANDARD}")

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wshadow")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wshadow")
endif()

# Debug: optimize for debugging experience (stack traces, no optimizations)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -fno-omit-frame-pointer -fno-optimize-sibling-calls"
    CACHE STRING "Compiler flags for Debug build" FORCE)

# Release: optimize for maximum performance
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -DNDEBUG"
    CACHE STRING "Compiler flags for Release build" FORCE)
#set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto=auto"
#    CACHE STRING "Linker flags for Release build" FORCE)

set(CMAKE_CXX_FLAGS_ASAN "-g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
    CACHE STRING "Compiler flags in asan build" FORCE)

set(CMAKE_CXX_FLAGS_TSAN "-g -O1 -fsanitize=thread -fno-sanitize-recover=all -fno-omit-frame-pointer"
    CACHE STRING "Compiler flags in tsan build" FORCE)

set(CMAKE_CXX_FLAGS_MSAN "-g -O1 -fsanitize=memory -fsanitize-recover=all -fno-omit-frame-pointer"
    CACHE STRING "Compiler flags in msan build" FORCE)

set(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS_ASAN} -fprofile-instr-generate -fcoverage-mapping")
