#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ $# -lt 2 ]]; then
    echo "Usage: script/convert.sh <input_csv> <output_columnar>" >&2
    exit 2
fi

INPUT_CSV="$1"
COLUMNAR="$2"
SCHEMA="${ROOT_DIR}/hits.schema"

BUILD_DIR="${ROOT_DIR}/cmake-build-release"
BIN="${BUILD_DIR}/columnar-engine"

if [[ ! -x "${BIN}" ]]; then
    echo "ERROR: columnar-engine not found at ${BIN}" >&2
    echo "Run script/build.sh first" >&2
    exit 1
fi

if [[ ! -f "${INPUT_CSV}" ]]; then
    echo "ERROR: input CSV not found: ${INPUT_CSV}" >&2
    exit 2
fi

if [[ ! -f "${SCHEMA}" ]]; then
    echo "ERROR: schema file not found: ${SCHEMA}" >&2
    exit 2
fi

mkdir -p "$(dirname "${COLUMNAR}")"

"${BIN}" convert "${INPUT_CSV}" "${COLUMNAR}" "${SCHEMA}"
