# README lib60870

lib60870 library for IEC 60870-5 based protocols in C

The current implementation contains code for the IEC 60870-5-104 (CS 104) TCP/IP based protocol
and the serial link layers (balanced and unbalanced) as defined in IEC 60870-5-101 (CS 101).

Please also consider the User Guide.


## Compiling and running the examples:

lib60870-C:

In the lib60870-C folder build the library with

`make`

Go to the subdirectories of the examples folder

and build the examples with

`make`

in each examples' directory.

The library and examples can also be build with _CMake_.

To build the library in a separate folder create a new folder as subdirectory of
the project folder and run cmake to create the build files:

`mkdir build`
`cd build`
`cmake ..`


## Contact:

The library is developed and supported my MZ Automation GmbH.

For bug reports, hints or support please contact info@mz-automation.de

## Licensing

This software is licensed under the GPLv3 (https://www.gnu.org/licenses/gpl-3.0.en.html).

## Commercial licenses and support

Support and commercial license options are provided by MZ Automation GmbH. Please contact info@mz-automation.de for more details.

## Contributing

If you want to contribute to the improvement and development of the library please send me comments, feature requests, bug reports, or patches. For more than trivial contributions I require you to sign a Contributor License Agreement. Please contact info@libiec61850.com.
