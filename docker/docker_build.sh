#!/bin/bash
# usage: docker_build.sh <cmd run inside container after builds>
set -e
docker run --rm -v /Users/uday/Binancer:/src:ro binance-verify:22.04 bash -c '
set -e
cp -r /src /w && cd /w && rm -rf build build-*
for cc in "g++-12" "clang++-15"; do
  echo "=== $cc"
  CXX=$cc cmake -G Ninja -B build-$cc -S . >/dev/null
  cmake --build build-$cc 2>&1 | tee /tmp/log-$cc | grep -E "warning|error" || true
  echo "warnings: $(grep -c "warning:" /tmp/log-$cc || true)"
  ./build-$cc/tests/unit_tests | tail -1
done
'"$1"
