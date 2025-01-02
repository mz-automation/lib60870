# README

Please add optional dependencies in this folder

## TLS Support

At the moment there are two different options for TLS support.

* mbedtls 2.28 supports TLS version 1.2, 1.1 and older versions of TLS
* mbedtls 3.6 supports TLS versions 1.2 and 1.3

### mbedtls 2.28

For TLS support with mbedtls 2.28 download the source tarball of version 2.28.x and extract here in the subfolder (Version 2.28.9 https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-2.28.9/mbedtls-2.28.9.tar.bz2)

Rename the extracted mbedtls folder to mbedtls-2.28 (otherwise the build script cannot find the folder and will compile the library without TLS support).


On a Linux command line enter the following commands in this directory:

    wget https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-2.28.9/mbedtls-2.28.9.tar.bz2
    tar xfj mbedtls-2.28.9.tar.bz2
    mv mbedtls-2.28.9 mbedtls-2.28

When using make the make command has to be invoked with the WITH_MBEDTLS=1 parameter

    make WITH_MBEDTLS=1

### mbedtls 3.6

For TLS support with mbedtls 3.6 download the source tarball of version 3.6.x and extract here in the subfolder (Version 3.6.2 https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.2/mbedtls-3.6.2.tar.bz2)

On a Linux command line enter the following commands in this directory:

    wget https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.2/mbedtls-3.6.2.tar.bz2
    tar xfj mbedtls-3.6.2.tar.bz2
    mv mbedtls-3.6.2 mbedtls-3.6

When running cmake the build system will automatically find the mbedtls source code and includes it into the library build.

When using make the make command has to be invoked with the WITH_MBEDTLS3=1 parameter

    make WITH_MBEDTLS3=1

