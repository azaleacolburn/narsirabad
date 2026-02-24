# build:
#     rm ./target/*
#     cc -c -fPIC narsirabad.c -o target/narsirabad.o
#     cc -c -fPIC mem.c -o target/mem.o
#     cc -shared target/narsirabad.o target/mem.o -o target/libnar.so

build:
    cc test/main.c narsirabad.c mem.c -o target/main
    # set -x LD_LIBRARY_PATH=/home/azalea/projects/narsirabad/target:$LD_LIBRARY_PATH
    # cc test/main.c -L. -lnar -o test/main
