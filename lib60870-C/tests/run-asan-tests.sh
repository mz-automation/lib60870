#!/usr/bin/env bash

set -u

usage()
{
    echo "Usage: $0 <test-executable> <log-file>" >&2
}

if [ "$#" -ne 2 ]; then
    usage
    exit 2
fi

test_executable=$1
log_file=$2
script_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)

case "$log_file" in
    /*) ;;
    *) log_file="$(pwd -P)/$log_file" ;;
esac

log_directory=$(dirname -- "$log_file")
mkdir -p -- "$log_directory"

asan_options=${ASAN_OPTIONS:-detect_leaks=1:halt_on_error=1:abort_on_error=1}

set +e
{
    echo "ASan options: $asan_options"
    echo "ASan log: $log_file"
    ASAN_OPTIONS="$asan_options" \
        bash "$script_directory/run-tests.sh" "$test_executable"
} 2>&1 | tee "$log_file"
statuses=("${PIPESTATUS[@]}")
set -e

test_status=${statuses[0]}
tee_status=${statuses[1]}

if [ "$tee_status" -ne 0 ]; then
    echo "Failed to write ASan test log: $log_file" >&2
    exit "$tee_status"
fi

exit "$test_status"
