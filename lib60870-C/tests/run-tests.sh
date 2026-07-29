#!/usr/bin/env bash

set -u

usage()
{
    echo "Usage: $0 <test-executable> <expected-test-count>" >&2
}

if [ "$#" -ne 2 ]; then
    usage
    exit 2
fi

test_executable=$1
expected_test_count=$2

case "$expected_test_count" in
    ''|*[!0-9]*)
        echo "Expected test count must be a non-negative integer: $expected_test_count" >&2
        exit 2
        ;;
esac

if [ ! -f "$test_executable" ]; then
    echo "Test executable not found: $test_executable" >&2
    exit 2
fi

if [ ! -x "$test_executable" ]; then
    echo "Test file is not executable: $test_executable" >&2
    exit 2
fi

test_directory=$(CDPATH= cd -- "$(dirname -- "$test_executable")" && pwd -P)
test_name=$(basename -- "$test_executable")
output_file=$(mktemp)

cleanup()
{
    rm -f -- "$output_file"
}

trap cleanup EXIT HUP INT TERM

echo "Running $test_name from $test_directory (expected tests: $expected_test_count)"

set +e
(
    cd -- "$test_directory"
    "./$test_name"
) 2>&1 | tee "$output_file"
test_status=${PIPESTATUS[0]}
set -e

summary=$(grep -E '^[[:space:]]*[0-9]+ Tests?[[:space:]]+[0-9]+ Failures?' "$output_file" | tail -n 1 || true)

if [ -z "$summary" ]; then
    echo "Unity test summary was not found in the test output." >&2
    exit 1
fi

actual_test_count=$(printf '%s\n' "$summary" | awk '{print $1}')
failure_count=$(printf '%s\n' "$summary" | awk '{print $3}')

if [ "$actual_test_count" -ne "$expected_test_count" ]; then
    echo "Unexpected test count: expected $expected_test_count, ran $actual_test_count." >&2
    exit 1
fi

if [ "$failure_count" -ne 0 ]; then
    echo "Unity reported $failure_count test failure(s)." >&2
    exit 1
fi

if [ "$test_status" -ne 0 ]; then
    echo "Test executable exited with status $test_status." >&2
    exit "$test_status"
fi

echo "Validated Unity result: $actual_test_count tests, 0 failures."
