# README lib60870

lib60870 library for IEC 60870-5 based protocols in C and C#

The current implementation contains code for the IEC 60870-5-104 protocol only.

Support for other protocol parts (companion standards - CS) like IEC 60870-5-101 (serial line link layer), IEC 60870-5-102 (CS for electricity meters) and IEC 60870-5-103 (CS for protection equipment) will be provided in future versions.

Please also consider the User Guide.


## Compiling and running the examples:


lib60870.NET:

Open the provided solution file in the lib60870.NET folder with MonoDevelop or Visual Studio. You should be able to build and run the library and examples with any recent version of MonoDevelop or Visual Studio

lib60870-C:

In the lib60870-C folder build the library with

`make`

Go to the examples folder

examples/simple_client
examples/simple_server

and build the examples with

`make`

in each examples' directory.


## Contact:

The library is developed and supported my MZ Automation GmbH.

For bug reports, hints or support please contact info@mz-automation.de

## Licensing

This software is licensed under the GPLv3 (https://www.gnu.org/licenses/gpl-3.0.en.html).

## Commercial licenses and support

Support and commercial license options are provided by MZ Automation GmbH. Please contact info@mz-automation.de for more details.

## Contributing

If you want to contribute to the improvement and development of the library please send me comments, feature requests, bug reports, or patches. For more than trivial contributions I require you to sign a Contributor License Agreement. Please contact info@libiec61850.com.
