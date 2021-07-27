# this one is important
SET(CMAKE_SYSTEM_NAME RPI3-linux-arm-eabi)
#this one not so much
SET(CMAKE_SYSTEM_VERSION 1)

# specify the cross compiler
SET(CMAKE_C_COMPILER   /opt/toolchains/gcc-linaro-7.5.0-2019.12-i686_armv8l-linux-gnueabihf/bin/armv8l-linux-gnueabihf-gcc)
SET(CMAKE_CXX_COMPILER /opt/toolchains/gcc-linaro-7.5.0-2019.12-i686_armv8l-linux-gnueabihf/bin/armv8l-linux-gnueabihf-g++)


# where is the target environment
SET(CMAKE_FIND_ROOT_PATH  /opt/toolchains/gcc-linaro-7.5.0-2019.12-i686_armv8l-linux-gnueabihf)

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
