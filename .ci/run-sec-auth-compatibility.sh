#!/usr/bin/env bash

set -euo pipefail

sec_auth_branch=${1:-main}
result_directory=${2:-}
sec_auth_repository_url=${SEC_AUTH_REPOSITORY_URL:-git@bitbucket.org:mz-automation/lib60870-sec-auth.git}
lib60870_repository_directory=$(git rev-parse --show-toplevel)
lib60870_commit=$(git -C "$lib60870_repository_directory" rev-parse HEAD)
temporary_directory=$(mktemp -d)
lib60870_source_directory="$temporary_directory/lib60870"
sec_auth_source_directory="$temporary_directory/lib60870-sec-auth"
build_directory="$temporary_directory/build"

cleanup()
{
    rm -rf "$temporary_directory"
}

trap cleanup EXIT INT TERM

if [ -n "$result_directory" ]; then
    case "$result_directory" in
        /*) ;;
        *) result_directory="$lib60870_repository_directory/$result_directory" ;;
    esac

    mkdir -p "$result_directory"
fi

echo "Validating lib60870 commit $lib60870_commit"
git clone --no-local --no-checkout \
    "$lib60870_repository_directory" "$lib60870_source_directory"
git -C "$lib60870_source_directory" checkout --detach "$lib60870_commit"

echo "Cloning lib60870-sec-auth branch $sec_auth_branch"

GIT_SSH_COMMAND="ssh -o StrictHostKeyChecking=accept-new" \
    git clone --depth 1 --branch "$sec_auth_branch" \
        "$sec_auth_repository_url" "$sec_auth_source_directory"

echo "Using Sec-Auth commit $(git -C "$sec_auth_source_directory" rev-parse HEAD)"

sec_auth_target_directory="$lib60870_source_directory/lib60870-C/modules/sec-auth"

if [ -e "$sec_auth_target_directory" ]; then
    echo "The Sec-Auth module path already exists: $sec_auth_target_directory" >&2
    exit 2
fi

mkdir -p "$sec_auth_target_directory"
cp -a "$sec_auth_source_directory/sec-auth/." "$sec_auth_target_directory/"

mbedtls_version=$(curl -sf \
    "https://api.github.com/repos/Mbed-TLS/mbedtls/releases?per_page=100" \
    | python3 -c "import sys,json,re;d=json.load(sys.stdin);p=re.compile(r'^mbedtls-3[.]6[.](\\d+)$');v=sorted([(int(m.group(1)),r['tag_name'].replace('mbedtls-','')) for r in d if not r['prerelease'] for m in [p.match(r['tag_name'])] if m],reverse=True);print(v[0][1])" \
    2>/dev/null) || true

[ -n "$mbedtls_version" ] || mbedtls_version=3.6.7
echo "Using mbedTLS $mbedtls_version"

wget -q \
    "https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$mbedtls_version/mbedtls-$mbedtls_version.tar.bz2" \
    -O "$temporary_directory/mbedtls.tar.bz2"
tar xfj "$temporary_directory/mbedtls.tar.bz2" \
    -C "$lib60870_source_directory/lib60870-C/dependencies/"

cmake -S "$lib60870_source_directory/lib60870-C" -B "$build_directory" \
    -DBUILD_EXAMPLES=OFF -DBUILD_TESTS=ON
cmake --build "$build_directory" --target sec_auth_tests --parallel "$(nproc)"

test_directory="$build_directory/modules/sec-auth/tests"

if [ -n "$result_directory" ]; then
    (cd "$test_directory" && ./sec_auth_tests) 2>&1 \
        | tee "$result_directory/sec-auth-tests.log"
else
    (cd "$test_directory" && ./sec_auth_tests)
fi
