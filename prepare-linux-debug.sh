#!/bin/bash
cmake -B build -DMODEM_DEBUG=ON -DCMAKE_BUILD_TYPE="Debug" $@ .
