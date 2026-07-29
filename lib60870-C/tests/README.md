# Running the C test suite

The C tests use Unity and are compiled into a single executable. Building the
`tests` target only creates that executable; it does not execute the tests.

`run-tests.sh` runs the executable from its own directory so the certificate
files copied by CMake can be found. It also checks the Unity summary and fails
when a test fails or when the number of executed tests is not the expected
number for the selected TLS configuration.

## Expected test counts

| Configuration | Expected tests |
| --- | ---: |
| No TLS | 94 |
| mbedTLS 2.28.x | 135 |
| mbedTLS 3.6.x | 134 |

The counts differ because some TLS conformance cases are specific to the
protocol versions and behavior provided by each mbedTLS series.

## CMake

From the repository root:

```sh
cmake -S lib60870-C -B build -DBUILD_EXAMPLES=OFF
cmake --build build --parallel
bash lib60870-C/tests/run-tests.sh build/tests/tests 94
```

Extracting an mbedTLS source tree under `lib60870-C/dependencies` enables TLS
automatically. Use `135` for an `mbedtls-2.28.x` tree or `134` for an
`mbedtls-3.6.x` tree.

## Make

From the repository root:

```sh
make -C lib60870-C lib tests
bash lib60870-C/tests/run-tests.sh lib60870-C/build/tests.exe 94
```

The Makefile also detects supported mbedTLS source directories under
`lib60870-C/dependencies`. The `.exe` suffix is part of the Makefile target name
on every platform, including Linux.

Secure Authentication and language-wrapper tests are separate suites and are
not included in these commands.
