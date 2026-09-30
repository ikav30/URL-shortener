#!/usr/bin/env bash
# Build script for the URL Shortener.
# Usage:  ./build.sh        (from the project root, in a terminal where g++ is GCC 16)
set -e

mkdir -p build

# SQLite is ~250k lines -> compile it once into an object file, reuse after that.
if [ ! -f build/sqlite3.o ]; then
  echo "compiling SQLite (one-time, ~30s)..."
  gcc -c src/lib/sqlite3.c -o build/sqlite3.o -O2
fi

# Keep-alive tuning: without a high cap, httplib closes connections after N requests,
# which cripples throughput under load. TCP_NODELAY is set in code.
FLAGS="-std=c++17 -O2 -DNOMINMAX \
  -DCPPHTTPLIB_KEEPALIVE_MAX_COUNT=1000000 \
  -DCPPHTTPLIB_KEEPALIVE_TIMEOUT_SECOND=60 \
  -static -static-libgcc -static-libstdc++"

echo "building server..."
g++ $FLAGS src/main.cpp src/base62.cpp src/database.cpp build/sqlite3.o \
    -o build/server.exe -lws2_32

echo "building benchmark tool..."
g++ $FLAGS src/bench.cpp -o build/bench.exe -lws2_32

echo "done."
echo "  run server:  ./build/server.exe        then open http://localhost:8080"
echo "  benchmark:   MSYS_NO_PATHCONV=1 ./build/bench.exe /1 8 5000"
