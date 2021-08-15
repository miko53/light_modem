#!/bin/bash
cmake -B build-linux-debug -DMODEM_DEBUG=ON -DCMAKE_INSTALL_PREFIX=../install_test/ -DCMAKE_BUILD_TYPE="Debug" $@ .
