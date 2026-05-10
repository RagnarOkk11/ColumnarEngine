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

template <typename ColumnClass>
class ConstantFunction : public ScalarFunction {
    using ValueType = ColumnClass::ValueType;
    using BuilderType = BuilderTypeTrait<ColumnClass>::Type;

public:
    ConstantFunction(ColumnType type, ValueType constant)
        : type_(type), constant_(std::move(constant)) {}

    std::shared_ptr<Column> Evaluate(const RecordBatch& batch) override {
        size_t physical_size = batch.num_rows;

        auto builder = ColumnFactory::MakeColumnBuilder(type_);
        auto* typed_builder = static_cast<BuilderType*>(builder.get());

        for (size_t i = 0; i < physical_size; ++i) {
            typed_builder->AddValue(constant_);
        }

        return builder->Finish();
    }

private:
    ColumnType type_;
    ValueType constant_;
};

template <typename ColumnClass>
class AddConstantFunction : public ScalarFunction {
    using ValueType = ColumnClass::ValueType;

public:
    AddConstantFunction(size_t col_ind, ValueType constant)
        : col_ind_(col_ind), constant_(std::move(constant)) {}

    std::shared_ptr<Column> Evaluate(const RecordBatch& batch) override {
        size_t physical_size = batch.columns.empty() ? batch.num_rows : batch.columns[0]->Size();
        std::vector<ValueType> result_data(physical_size);

        AggExpHelper::IterateColumnData<ColumnClass>(
            batch, col_ind_, [&](const auto& value, size_t, size_t real_ind) {
                result_data[real_ind] = static_cast<ValueType>(value + constant_);
            });

        return std::make_shared<ColumnClass>(std::move(result_data));
    }

private:
    size_t col_ind_;
    ValueType constant_;
};
