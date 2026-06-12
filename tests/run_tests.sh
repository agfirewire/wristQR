#!/bin/sh
set -e
cd "$(dirname "$0")/.."
mkdir -p build-host
cc -std=c11 -Wall -Wextra -Werror -I src/c -o build-host/test_model tests/test_model.c src/c/model.c
./build-host/test_model
if [ -f tests/test_qr_layout.c ]; then
  cc -std=c11 -Wall -Wextra -Werror -I src/c -o build-host/test_qr_layout tests/test_qr_layout.c src/c/qr_layout.c
  ./build-host/test_qr_layout
fi
echo "ALL TESTS PASSED"
