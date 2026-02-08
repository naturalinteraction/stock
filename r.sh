#!/bin/bash
cd "$(dirname "$0")" && make && ./stockchart "$@"
