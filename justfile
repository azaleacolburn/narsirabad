build:
    cc test/main.c alloc.c gc.c mem.c -o target/main -Wall -Werror -Wpedantic

test:
    ./target/main

build-prod:
    mkdir export
    rm export/*

    cc -c -fPIC alloc.c -o target/narsirabad.o
    cc -c -fPIC gc.c -o target/gc.o
    cc -c -fPIC mem.c -o target/mem.o
    cc -shared target/alloc.o target/gc.o target/mem.o -o target/libnar.so

test-prod:
    set -x LD_LIBRARY_PATH=/home/azalea/projects/narsirabad/target:$LD_LIBRARY_PATH
    cc test/main.c -L. -lnar -o test/main

