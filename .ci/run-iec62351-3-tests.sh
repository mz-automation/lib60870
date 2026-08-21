#!/usr/bin/env bash

set -eu

test_branch=${1:-main}
results_directory=${2:-iec62351-3-results}
test_repository_url=${IEC62351_TEST_REPOSITORY_URL:-git@bitbucket.org:mz-automation/iec62351_3_tls_tests.git}
base_repository=$(git rev-parse --show-toplevel)
temporary_directory=$(mktemp -d)
base_source_directory="$temporary_directory/lib60870"
test_source_directory="$temporary_directory/iec62351_3_tls_tests"
test_build_directory="$temporary_directory/build"

case "$results_directory" in
    /*) ;;
    *) results_directory="$base_repository/$results_directory" ;;
esac

if [ -e "$results_directory" ]; then
    echo "Results path already exists: $results_directory" >&2
    exit 2
fi

cleanup()
{
    rm -rf "$temporary_directory"
}

trap cleanup EXIT INT TERM

echo "Validating lib60870 commit $(git -C "$base_repository" rev-parse HEAD)"
echo "Cloning iec62351_3_tls_tests branch $test_branch"

GIT_SSH_COMMAND="ssh -o StrictHostKeyChecking=accept-new" \
    git clone --depth 1 --branch "$test_branch" "$test_repository_url" "$test_source_directory"

echo "Using iec62351_3_tls_tests commit $(git -C "$test_source_directory" rev-parse HEAD)"

mkdir -p "$base_source_directory"
git -C "$base_repository" archive HEAD | tar -x -C "$base_source_directory"

(
    cd "$test_source_directory/tls_libraries"
    bash apply_mbedtls_patch.sh
)

bash "$test_source_directory/scripts/generate-certificates.sh" \
    "$test_source_directory/build/generated-certs"

ln -s "$test_source_directory/tls_libraries/mbedtls-2.28-modified" \
    "$base_source_directory/lib60870-C/dependencies/mbedtls-2.28.3"

cmake -S "$test_source_directory" -B "$test_build_directory" \
    -DCMAKE_BUILD_TYPE=Release \
    -DTEST_CERTIFICATE_DIR="$test_source_directory/build/generated-certs" \
    -DLIB60870_SOURCE_DIR="$base_source_directory/lib60870-C"

cmake --build "$test_build_directory" --parallel "$(nproc)"

test_status=0
bash "$test_source_directory/scripts/run-reference-tests.sh" "$test_build_directory" || test_status=$?

mkdir -p "$results_directory"
cp -a "$test_build_directory/reference-results/." "$results_directory/"

echo "Stored IEC 62351-3 test logs in $results_directory"
exit "$test_status"
