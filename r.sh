#!/bin/bash
cd "$(dirname "$0")" && make -j16 && ./bin/stock "$@"
