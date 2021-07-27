

cmake_minimum_required(VERSION 3.13)

### Std compiler options
if(UNIX)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=gnu99 -W -Wall -Wno-aggregate-return -Wno-suggest-attribute=format -Wno-undef -fms-extensions")  #-Wno-discarded-qualifiers
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -fstack-protector-all -Wstack-protector -fno-omit-frame-pointer")
endif()

include_directories(include)

add_subdirectory(src)
add_subdirectory(tools)
