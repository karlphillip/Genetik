#!/usr/bin/env bash
set -euo pipefail
expected_arch=$1
minimum=$2
git rev-parse HEAD
uname -a
cat /etc/os-release
printf 'runner_os=%s runner_arch=%s image=%s version=%s\n' "${RUNNER_OS:-local}" "${RUNNER_ARCH:-local}" "${ImageOS:-unknown}" "${ImageVersion:-unknown}"
test "$(uname -m)" = "$expected_arch"
"$CC" --version
"$CXX" --version
"$CXX" -dumpmachine
cmake --version
ctest --version
ldd --version
python3 --version
dpkg-query -W -f='${binary:Package} ${Version}\n' libc6 libstdc++6 libtsan2
"$CXX" -print-file-name=libstdc++.so
printf '#include <version>\n' | "$CXX" -std=c++20 -dM -E -x c++ - | grep -E '^#define (__cplusplus|__GLIBCXX__|_GLIBCXX_RELEASE|_LIBCPP_VERSION|__GNUC__|__clang_major__|__SIZEOF_POINTER__) ' | sort
if [[ "$CXX" == 'g++-13' ]]; then
  test "$("$CXX" -dumpversion)" = 13
else
  "$CXX" --version | head -1 | grep -E 'version 18\.'
fi
if [[ "$minimum" == 'true' ]]; then
  test "$(cmake --version | head -1)" = 'cmake version 3.16.0'
fi
