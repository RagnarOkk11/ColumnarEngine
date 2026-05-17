#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ $# -lt 4 ]]; then
    echo "Usage: script/run_query_mt.sh <query_num> <columnar> <output_csv> <log_file>" >&2
    exit 2
fi

QUERY_NUM="$1"
COLUMNAR="$2"
OUTPUT_CSV="$3"
LOG_FILE="$4"

BUILD_DIR="${ROOT_DIR}/cmake-build-release"
BIN="${BUILD_DIR}/columnar-engine-mt"

if [[ ! -x "${BIN}" ]]; then
    echo "ERROR: columnar-engine-mt not found at ${BIN}" >&2
    echo "Run script/build.sh first" >&2
    exit 1
fi

if [[ ! -f "${COLUMNAR}" ]]; then
    echo "ERROR: columnar file not found: ${COLUMNAR}" >&2
    exit 2
fi

mkdir -p "$(dirname "${OUTPUT_CSV}")"
mkdir -p "$(dirname "${LOG_FILE}")"

# Run the query, capture stdout (CSV result) and tee stderr+stdout to log
"${BIN}" query "${COLUMNAR}" "${QUERY_NUM}" > "${OUTPUT_CSV}" 2> >(tee "${LOG_FILE}" >&2)
