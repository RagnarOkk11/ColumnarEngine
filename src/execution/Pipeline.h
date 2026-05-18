#pragma once

#include "column/Column.h"
#include "utils/ThreadPool.h"

#include <memory>
#include <vector>

struct RecordBatch {
    size_t num_rows;
    std::shared_ptr<const std::vector<size_t>> selection_vector;
    std::vector<std::shared_ptr<Column>> columns;
};

// generates RecordBatches (for example scan operator)
class SourceOperator {
public:
    virtual ~SourceOperator() = default;
    virtual std::unique_ptr<RecordBatch> GetData() = 0;
};

// transforms per-batch data
class TransformOperator {
public:
    virtual ~TransformOperator() = default;
    virtual std::unique_ptr<RecordBatch> Execute(std::unique_ptr<RecordBatch> batch) = 0;

    virtual bool IsPipelineDone() const {
        return false;
    }
};

// accumulates all data, breaks pipeline
class SinkOperator {
public:
    virtual ~SinkOperator() = default;

    virtual void Sink(std::unique_ptr<RecordBatch> batch, size_t thread_id) = 0;
    virtual void Finalize() && = 0;
};

class Pipeline {
public:
    std::shared_ptr<SourceOperator> source;
    std::vector<std::shared_ptr<TransformOperator>> transforms;
    std::shared_ptr<SinkOperator> sink;

    void Execute(ThreadPool& thread_pool, size_t num_threads) {
        size_t num_tasks_finished{0};
        Mutex wait_mutex;
        ConditionVariable wait_cond;
        for (size_t thread_id = 0; thread_id < num_threads; ++thread_id) {
            thread_pool.Enqueue(
                [this, thread_id, &num_tasks_finished, &wait_mutex, &wait_cond, num_threads]() {
                    while (auto batch = source->GetData()) {
                        bool skip = false;
                        bool stop = false;

                        for (std::shared_ptr<TransformOperator>& transform : transforms) {
                            batch = transform->Execute(std::move(batch));
                            if (!batch || batch->num_rows == 0) {
                                skip = true;
                                break;
                            }
                            if (transform->IsPipelineDone()) {
                                stop = true;
                                break;
                            }
                        }

                        if (skip) {
                            continue;
                        }
                        if (batch && batch->num_rows > 0) {
                            sink->Sink(std::move(batch), thread_id);
                        }
                        if (stop) {
                            break;
                        }
                    }
                    {
                        UniqueLock<Mutex> lock(wait_mutex);
                        ++num_tasks_finished;
                        if (num_tasks_finished == num_threads) {
                            wait_cond.notify_one();
                        }
                    }
                });
        }

        {
            UniqueLock<Mutex> lock(wait_mutex);
            wait_cond.wait(lock, [&]() {
                return num_tasks_finished == num_threads;
            });
        }

        std::move(*sink).Finalize();
    }
};
