
Static phase is in DangPointer directory.

Build Transformation phase:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    $ cd ..

Run:

    $ clang -Xclang -load -Xclang build/src/libNoDangPass.so something.c

Test:

    Some examples are in directory named as test.
