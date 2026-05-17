#pragma once

#include "execution/Pipeline.h"
#include "column/Schema.h"

#include <memory>
#include <vector>

class ResultSinkOperator : public SinkOperator {
public:
    void Sink(std::unique_ptr<RecordBatch> batch, size_t /* thread_id */) override {
        LockGuard<Mutex> lock(mutex_);
        batches_.push_back(std::move(batch));
    }

    void Finalize() && override {
        // charonchik bebe
    }

    std::vector<std::unique_ptr<RecordBatch>> TakeBatches() {
        return std::move(batches_);
    }

private:
    Mutex mutex_;
    std::vector<std::unique_ptr<RecordBatch>> batches_;
};

struct PipelineBuildContext {
    Pipeline* current_pipeline = nullptr;
    std::vector<std::unique_ptr<Pipeline>> completed_pipelines;
    Schema schema;
    size_t num_threads = 1;
};
