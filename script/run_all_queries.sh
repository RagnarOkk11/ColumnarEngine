#!/bin/bash

set -e

cd "$(dirname "$0")" || exit 1

echo ">>> Запуск setup.sh..."
./setup.sh

echo ">>> Запуск build.sh..."
./build.sh

echo ">>> Запуск convert.sh..."
./convert.sh ../hits_sample.csv ../columnar_hits_sample.tuff ../hits.schema

echo ">>> Запуск запросов от 0 до 42..."
for i in {0..42}
do
    echo "========================================="
    echo ">>> Выполнение запроса $i"
    echo "========================================="
    ./run_query.sh "$i" ../columnar_hits_sample.tuff test.csv test.log
done

echo ">>> Все запросы выполнены успешно."