#!/bin/bash
gitk &
cd "$(dirname "$0")" && make -j16 && ./bin/stock "$@"
