# light_modem - a minimal version of xmodem and ymodem.

## 1. DESCRIPTION

This library is a minimalist implementation of xmodem and ymodem.
It aims to be adaptable to a baremetal target with file in RAM.
Some callback are used to provided serial data and buffering.

## 2. FEATURE

xmodem (block of 128 bytes and checksum)
xmodem-crc (block of 128 bytes but with a crc)
xmodem-1k (block of 1024 bytes with a crc)
ymodem (with adaptation of file size and api to retrieve file characteristics)


## 3. COMPILATION

use cmake
see bach script at the root, for example `prepare-linux-debug.sh` and `prepare-linux-release.sh`

```
$ ./prepare-linux-debug.sh
$ cd build
$ make all
$ cd ../tests
$ ./launch_tests.rb
```

for installation `./prepare-linux-debug.sh -DCMAKE_INSTALL_PREFIX=<install_path>` and finally
`make all install`

## 4. TESTS

library has been tests with minicom.
a test serie is available in `tests` folder, it is based on linux version by simulating serial line with `socat`
a tool `rzsz` is used to perform tests.
see script in `tests/launch_tests.rb`

## 5. TODO

- add tests for retry and NAK reception
- add callback to read/write data on-the-fly and not in ram if necessary
- add arguments for tests for release/debug version
