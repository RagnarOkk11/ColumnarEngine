#pragma once

#include "execution/OperatorsBase.h"
#include "execution/ExecutionHelper.h"
#include "column/TemporalColumn.h"
#include "column/NumericColumn.h"

#include <memory>

class ScalarFunction {
public:
    virtual ~ScalarFunction() = default;
    virtual std::shared_ptr<Column> Evaluate(const RecordBatch& batch) = 0;
};

class ExtractMinuteFunction : public ScalarFunction {
public:
    explicit ExtractMinuteFunction(size_t col_ind) : col_ind_(col_ind) {}

    std::shared_ptr<Column> Evaluate(const RecordBatch& batch) override {
        std::vector<int32_t> result_data;
        result_data.reserve(batch.num_rows);

        ExecutionHelper::IterateColumnData<TimestampColumn>(batch, col_ind_, [&](const auto& value, size_t, size_t) {
            result_data.push_back(Timestamp::ExtractMinute(value));
        });

        auto result_col = std::make_shared<Int32Column>(std::move(result_data));
        return result_col;
    }

private:
    size_t col_ind_;
};
