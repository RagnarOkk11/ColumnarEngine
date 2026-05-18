#!/bin/bash
set -e

# usage:
#   -i 3 - count of iterations (or --iterations)
#   --answers - print answers for each query
#   --debug - print time of execution for each query

cd "$(dirname "$0")" || exit 1

# Значения по умолчанию
ITERATIONS=1
SHOW_DEBUG=0
SHOW_ANSWERS=0

# Парсинг аргументов командной строки
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --debug) SHOW_DEBUG=1; shift ;;
        --answers) SHOW_ANSWERS=1; shift ;;
        -i|--iterations) ITERATIONS="$2"; shift 2 ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
done

echo ">>> Running queries 0-42 (multithreaded)..."
echo ">>> Iterations: $ITERATIONS | Show Debug: $SHOW_DEBUG | Show Answers: $SHOW_ANSWERS"

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

        CSV_FILE="${RESULTS_DIR}/query_${i}_iter_${iter}.csv"
        LOG_FILE="${RESULTS_DIR}/query_${i}_iter_${iter}.log"

        set +e
        ./run_query_mt.sh "$i" ../columnar_hits_sample.tuff "$CSV_FILE" "$LOG_FILE"
        exit_code=$?
        set -e

        if [ $exit_code -ne 0 ]; then
            echo "FAILED with exit code $exit_code"
            echo "See log: $LOG_FILE"
            echo "--- CRASH LOG ---"
            cat "$LOG_FILE" # При падении всегда выводим лог, чтобы сразу видеть ошибку
            exit $exit_code
        fi
        echo "OK"

        # Если запрошен вывод ответов
        if [ "$SHOW_ANSWERS" -eq 1 ]; then
            echo "------ ANSWERS (Query $i) ------"
            cat "$CSV_FILE"
            echo "--------------------------------"
        fi

        # Если запрошен вывод дебаг информации (времени выполнения и т.д.)
        if [ "$SHOW_DEBUG" -eq 1 ]; then
            echo "------- DEBUG (Query $i) -------"
            cat "$LOG_FILE"
            echo "--------------------------------"
        fi
    done
done

echo ">>> All $ITERATIONS iterations of all queries completed successfully."