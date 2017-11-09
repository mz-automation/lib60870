# README lib60870-C

lib60870 library for IEC 60870-5 based protocols in C

The current implementation contains code for the IEC 60870-5-104 protocol only.

Support for other protocol parts (companion standards - CS) like IEC 60870-5-101 (serial line link layer), IEC 60870-5-102 (CS for electricity meters) and IEC 60870-5-103 (CS for protection equipment) will be provided in future versions.

Please also consider the User Guide.


## Compiling and running the examples:


In the lib60870-C folder build the library with

`make`

Go to the examples folder and build the examples with

`make`

in each examples' directory.

Alternatively you can use cmake to build the library and examples

## Building with TLS support

The library can be build with support for TLS. In order to do so you have to download mbedtls version 2.6.0.

Unpack the mbedtls tarball in the dependencies folder so that a folder

dependencies/mbedtls-2.6.0

exists.

The cmake build system will automatically detect the mbedtls source and build the library with TLS support and mbedtls included

When using make you have to call make with WITH_MBEDTLS=1

make WITH_MBEDTLS=1

## Contact:

The library is developed and supported my MZ Automation GmbH.

For bug reports, hints or support please contact info@mz-automation.de

## Licensing

This software is licensed under the GPLv3 (https://www.gnu.org/licenses/gpl-3.0.en.html).

## Commercial licenses and support

Support and commercial license options are provided by MZ Automation GmbH. Please contact info@mz-automation.de for more details.

## Contributing

If you want to contribute to the improvement and development of the library please send me comments, feature requests, bug reports, or patches. For more than trivial contributions I require you to sign a Contributor License Agreement. Please contact info@libiec61850.com.
