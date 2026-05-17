#!/bin/bash
set -e

cd "$(dirname "$0")" || exit 1

echo ">>> Running queries 0-42 multithreaded..."

ITERATIONS=${1:-1}
RESULTS_DIR="query_results_mt"
mkdir -p "${RESULTS_DIR}"

for (( iter=1; iter<=ITERATIONS; iter++ ))
do
    echo "========================================="
    echo ">>> Iteration $iter / $ITERATIONS"
    echo "========================================="
    for i in {0..42}
    do
        echo -n ">>> Executing query $i ... "
        set +e
        ./run_query_mt.sh "$i" ../columnar_hits_sample.tuff "${RESULTS_DIR}/query_${i}_iter_${iter}.csv" "${RESULTS_DIR}/query_${i}_iter_${iter}.log"
        exit_code=$?
        set -e
        
        if [ $exit_code -ne 0 ]; then
            echo "FAILED with exit code $exit_code"
            echo "See log: ${RESULTS_DIR}/query_${i}_iter_${iter}.log"
            exit $exit_code
        fi
        echo "OK"
    done
done

echo ">>> All $ITERATIONS iterations of all queries completed successfully."
