#!/bin/bash
set -euxo pipefail
cd "$(dirname "${0}")"
make
make check
sudo make check
cd tests
if which valgrind > /dev/null; then
    ./test_bindfs.rb --valgrind
    sudo ./test_bindfs.rb --valgrind
else
    echo "Valgrind not installed. Not running tests with Valgrind."
fi
