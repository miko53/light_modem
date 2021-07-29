#!/bin/bash
./build/tools/rzsz --protocol 0 --device /dev/ttyUSB0 --speed 115200 --nb-stop 1 --1k --crc $@
