#!/usr/bin/env bash
# Merge raw profile files collected on the phone into android/pgo/tlou.profdata.
# Usage: TLOU_NDK=/path/to/ndk/29.0.14206865 bash android/bionic/pgo_merge.sh <folder-with-profraw>
set -Eeuo pipefail
cd "$(dirname "$0")/../.."
ndk=${TLOU_NDK:-${ANDROID_NDK_HOME:-}}
: "${ndk:?Set TLOU_NDK or ANDROID_NDK_HOME (merge with the same NDK used by the instrumented APK)}"
tool=$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-profdata
[[ -x $tool ]] || { echo "llvm-profdata not found: $tool" >&2; exit 2; }
folder=${1:?Usage: pgo_merge.sh <folder-with-profraw>}
shopt -s nullglob
raw=("$folder"/*.profraw)
(("${#raw[@]}")) || { echo "No .profraw files in $folder" >&2; exit 2; }
mkdir -p android/pgo
"$tool" merge -sparse -o android/pgo/tlou.profdata "${raw[@]}"
echo "Wrote android/pgo/tlou.profdata from ${#raw[@]} file(s). Commit it to enable PGO (TLOU_ANDROID_PGO=auto)."
