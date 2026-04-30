#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${ROOT_DIR}/cmake-build-release"

mkdir -p "${BUILD_DIR}"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_CXX_STANDARD=23

cmake --build "${BUILD_DIR}" --target columnar-engine -j "$(nproc)"
