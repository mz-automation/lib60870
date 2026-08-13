# Running the C test suite

The C tests use Unity and are compiled into a single executable. Building the
`tests` target only creates that executable; it does not execute the tests.

`run-tests.sh` runs the executable from its own directory so the certificate
files copied by CMake can be found. It checks the Unity summary and fails when
a test fails or the summary is missing. The test count is read from the Unity
output rather than hard-coded, so adding a test does not require a pipeline
update.

## CMake

From the repository root:

```sh
cmake -S lib60870-C -B build -DBUILD_EXAMPLES=OFF
cmake --build build --parallel
bash lib60870-C/tests/run-tests.sh build/tests/tests
```

Extracting an mbedTLS source tree under `lib60870-C/dependencies` enables TLS
automatically.

## Make

From the repository root:

```sh
make -C lib60870-C lib tests
bash lib60870-C/tests/run-tests.sh lib60870-C/build/tests.exe
```

The Makefile also detects supported mbedTLS source directories under
`lib60870-C/dependencies`. The `.exe` suffix is part of the Makefile target name
on every platform, including Linux.

Secure Authentication and language-wrapper tests are separate suites and are
not included in these commands.

## AddressSanitizer

AddressSanitizer (ASan) detects invalid memory access and, on Linux, memory
leaks while the tests execute. It is a separate diagnostic build: do not mix
its compiler flags with coverage instrumentation or Valgrind.

For a no-TLS build using GCC or Clang on Linux:

```sh
cmake -S lib60870-C -B build-asan -DBUILD_EXAMPLES=OFF \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
cmake --build build-asan --parallel
bash lib60870-C/tests/run-asan-tests.sh \
  build-asan/tests/tests asan-results/no-tls.log
```

The runner enables leak detection and stops on the first sanitizer finding. As
with the standard build, placing a supported mbedTLS source directory under
`lib60870-C/dependencies` enables TLS automatically. The ASan runner validates
the Unity result without requiring a hard-coded test count, so adding tests
does not require a pipeline update.

The Bitbucket custom pipeline `asan-test-matrix` runs the no-TLS, latest
mbedTLS 2.28.x, and latest mbedTLS 3.6.x configurations in parallel. It is
started manually and publishes the complete output from each job as an
artifact. Valgrind remains a local diagnostic and is not part of that pipeline.
