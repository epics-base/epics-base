
Host builds can be invoked with no extra arguments (default prefix is /usr/local).  This can also be done interactively by using the 'ccmake' or 'cmake-gui' executables.

$ mkdir build
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX:FILEPATH=$PWD/../inst ..
$ make install

Cross targets can be specified as a ';' seperated list.  Target names are GNU toolchain names.  RTEMS names append the bsp name as an extra entry.  Tools are looked for in /usr/GNU_NAME.  This can be overridden globally by setting -DTOOL_PREFIX=/opt/gnu or per target by -Dpowerpc-rtems4.9-mvme3100_PREFIX=/opt/rtems

mingw32 (on gentoo)
i586-mingw32msvc (on debian)
powerpc-rtems4.9-mvme3100

eg.

$ mkdir build
$ cd build
$ cmake -DTARGETS=i586-mingw32msvc;powerpc-rtems4.9-mvme3100 \
   -DCMAKE_INSTALL_PREFIX:FILEPATH=$PWD/../inst ..
$ make install
