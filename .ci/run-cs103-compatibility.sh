#!/usr/bin/env bash

set -eu

cs103_branch=${1:-v2.2_develop}
cs103_repository_url=${CS103_REPOSITORY_URL:-git@bitbucket.org:mz-automation/lib60870-cs103.git}
base_source_directory=$(git rev-parse --show-toplevel)
temporary_directory=$(mktemp -d)
cs103_source_directory="$temporary_directory/lib60870-cs103"
cs103_build_directory="$temporary_directory/build"

cleanup()
{
    rm -rf "$temporary_directory"
}

trap cleanup EXIT INT TERM

echo "Validating lib60870 commit $(git -C "$base_source_directory" rev-parse HEAD)"
echo "Cloning lib60870-cs103 branch $cs103_branch"

GIT_SSH_COMMAND="ssh -o StrictHostKeyChecking=accept-new" \
    git clone --depth 1 --branch "$cs103_branch" "$cs103_repository_url" "$cs103_source_directory"

echo "Using lib60870-cs103 commit $(git -C "$cs103_source_directory" rev-parse HEAD)"

if [ -e "$cs103_source_directory/deps/lib60870" ]; then
    echo "The CS103 dependency path already exists: $cs103_source_directory/deps/lib60870" >&2
    exit 2
fi

ln -s "$base_source_directory" "$cs103_source_directory/deps/lib60870"

cmake -S "$cs103_source_directory" -B "$cs103_build_directory"
cmake --build "$cs103_build_directory" --target tests-cs103 --parallel "$(nproc)"

bash "$cs103_source_directory/.ci/run-cs103-tests.sh" \
    "$cs103_build_directory/cs103/tests/tests-cs103/tests-cs103" 38
