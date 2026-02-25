build:
    cc test/main.c narsirabad.c mem.c -o target/main -Wall -Werror -Wpedantic

test:
    ./target/main

build-prod:
    mkdir export
    rm export/*

    cc -c -fPIC narsirabad.c -o target/narsirabad.o
    cc -c -fPIC mem.c -o target/mem.o
    cc -shared target/narsirabad.o target/mem.o -o target/libnar.so

test-prod:
    set -x LD_LIBRARY_PATH=/home/azalea/projects/narsirabad/target:$LD_LIBRARY_PATH
    cc test/main.c -L. -lnar -o test/main

