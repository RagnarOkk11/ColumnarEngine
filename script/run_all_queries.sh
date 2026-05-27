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

echo ">>> Running queries 0-42 (single thread)..."
echo ">>> Iterations: $ITERATIONS | Show Debug: $SHOW_DEBUG | Show Answers: $SHOW_ANSWERS"

RESULTS_DIR="query_mt_results"
if [ "$SHOW_ANSWERS" -eq 1 ] || [ "$SHOW_DEBUG" -eq 1 ]; then
    mkdir -p "${RESULTS_DIR}"
fi

for (( iter=1; iter<=ITERATIONS; iter++ ))
do
    echo "========================================="
    echo ">>> Iteration $iter / $ITERATIONS"
    echo "========================================="
    for i in {0..42}
    do
        echo -n ">>> Executing query $i ... "

        # Куда пишем ответы
        if [ "$SHOW_ANSWERS" -eq 1 ]; then
            CSV_FILE="${RESULTS_DIR}/query_${i}_iter_${iter}.csv"
        else
            CSV_FILE="/dev/null"
        fi

        # Куда пишем логи (чтобы вытащить время)
        if [ "$SHOW_DEBUG" -eq 1 ]; then
            LOG_FILE="${RESULTS_DIR}/query_${i}_iter_${iter}.log"
        else
            LOG_FILE=$(mktemp) # Временный файл, который мы потом удалим
        fi

        set +e
        ./run_query_mt.sh "$i" ../columnar_hits_sample.tuff "$CSV_FILE" "$LOG_FILE"
        exit_code=$?
        set -e

        # Вытаскиваем время из лога (формат: "Query 0 completed in 2.57227 ms")
        EXEC_TIME=$(grep "completed in" "$LOG_FILE" | awk '{print $5, $6}' || true)

        if [ $exit_code -ne 0 ]; then
            echo "FAILED with exit code $exit_code"
            echo "--- CRASH LOG ---"
            cat "$LOG_FILE"
            if [ "$SHOW_DEBUG" -eq 0 ]; then rm -f "$LOG_FILE"; fi
            exit $exit_code
        fi

        # Красиво выводим время
        if [ -n "$EXEC_TIME" ]; then
            echo "${EXEC_TIME} ... OK"
        else
            echo "OK"
        fi

        if [ "$SHOW_ANSWERS" -eq 1 ]; then
            echo "------ ANSWERS (Query $i) ------"
            cat "$CSV_FILE"
            echo "--------------------------------"
        fi

        if [ "$SHOW_DEBUG" -eq 1 ]; then
            echo "------- DEBUG (Query $i) -------"
            cat "$LOG_FILE"
            echo "--------------------------------"
        else
            # Удаляем мусор, если дебаг не нужен
            rm -f "$LOG_FILE"
        fi
    done
done

echo ">>> All $ITERATIONS iterations of all queries completed successfully."