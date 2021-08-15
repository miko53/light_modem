#!/bin/bash
rm -rf build-linux-coverage
cmake -B build-linux-coverage -DCOVERAGE=ON -DMODEM_DEBUG=ON -DCMAKE_INSTALL_PREFIX=../install_test/ -DCMAKE_BUILD_TYPE="Debug" $@ .
cmake --build build-linux-coverage
cd tests/
./launch_tests.rb --custom_path=../build-linux-coverage/tools/rzsz
cd ../build-linux-coverage
lcov --directory . --capture --rc lcov_branch_coverage=1 --output-file lmodem_coverage.info
genhtml --output-directory coverage_results --demangle-cpp --num-spaces 4 --sort --title "lmodem coverage" --function-coverage --branch-coverage --legend lmodem_coverage.info
cd ..

firefox build-linux-coverage/coverage_results/index.html
