#pragma once

#include "execution/OperatorsBase.h"
#include "execution/expressions/AggExpHelper.h"
#include "column/TemporalColumn.h"
#include "column/NumericColumn.h"
#include "types/String.h"

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
        size_t physical_size = batch.columns[0]->Size();
        std::vector<int32_t> result_data(physical_size);

        AggExpHelper::IterateColumnData<TimestampColumn>(batch, col_ind_, [&](const auto& value, size_t, size_t real_ind) {
            result_data[real_ind] = Timestamp::ExtractMinute(value);
        });

        return std::make_shared<Int32Column>(std::move(result_data));
    }

private:
    size_t col_ind_;
};

class LengthFunction : public ScalarFunction {
public:
    explicit LengthFunction(size_t col_ind) : col_ind_(col_ind) {}

    std::shared_ptr<Column> Evaluate(const RecordBatch& batch) override {
        size_t physical_size = batch.columns[0]->Size();
        std::vector<int64_t> result_data(physical_size);

        AggExpHelper::IterateColumnData<StringColumn>(batch, col_ind_, [&](const auto& value, size_t, size_t real_ind) {
            result_data[real_ind] = String::Length(value);
        });

        return std::make_shared<Int64Column>(std::move(result_data));
    }

private:
    size_t col_ind_;
};
