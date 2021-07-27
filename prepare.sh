#!/bin/bash
cmake -B build-debug -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain_raspberry_linaro.cmake -DCMAKE_BUILD_TYPE="Debug" $@ .
