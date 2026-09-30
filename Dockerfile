# ---- build stage: compile a static Linux binary ----
FROM gcc:14 AS build
WORKDIR /app
COPY src ./src

# Compile SQLite once, then the app (statically linked -> no runtime deps).
RUN gcc -O2 -c src/lib/sqlite3.c -o sqlite3.o && \
    g++ -std=c++17 -O2 -pthread -static \
        -DCPPHTTPLIB_KEEPALIVE_MAX_COUNT=1000000 \
        -DCPPHTTPLIB_KEEPALIVE_TIMEOUT_SECOND=60 \
        src/main.cpp src/base62.cpp src/database.cpp sqlite3.o \
        -o urlshortener -ldl -lm

# ---- runtime stage: tiny image with just the binary + dashboard ----
FROM debian:stable-slim
WORKDIR /app
COPY --from=build /app/urlshortener ./urlshortener
COPY public ./public
EXPOSE 8080
CMD ["./urlshortener"]
