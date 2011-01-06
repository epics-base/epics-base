
Building host and cross mingw32

$ mkdir build
$ cd build
$ cmake -DTARGETS=mingw32 -DCMAKE_INSTALL_PREFIX:FILEPATH=$PWD/../inst ..
$ make install

Cross Build (RTEMS mvme3100)

$ mkdir build-rtems
$ cd build-rtems
$ cmake -DCMAKE_TOOLCHAIN_FILE=$PWD/../cmake/cross/Toolchain-rtems.cmake -DCMAKE_INSTALL_PREFIX:FILEPATH=$PWD/../rtems 
-DEPICS_HOST_BASE:FILEPATH=$PWD/../inst ..
