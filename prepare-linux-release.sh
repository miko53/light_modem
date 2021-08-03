#!/bin/bash
cmake -B build-linux-release -DCMAKE_BUILD_TYPE="Release" $@ .
