#pragma once

#include "execution/OperatorsBase.h"
#include "execution/expressions/AggExpHelper.h"
#include "column/ColumnView.h"
#include "column/TemporalColumn.h"
#include "column/NumericColumn.h"
#include "types/String.h"

#include <regex>
#include <memory>

class ScalarFunction {
public:
    virtual ~ScalarFunction() = default;
    virtual std::shared_ptr<Column> Evaluate(const RecordBatch& batch) = 0;
};

// TODO: remove ExtractMinute, make Extract with uniform interface (like TruncateFunction)
class ExtractMinuteFunction : public ScalarFunction {
public:
    explicit ExtractMinuteFunction(size_t col_ind) : col_ind_(col_ind) {
    }

    std::shared_ptr<Column> Evaluate(const RecordBatch& batch) override {
        size_t physical_size = batch.columns[0]->Size();
        std::vector<int32_t> result_data(physical_size);

        AggExpHelper::IterateColumnData<TimestampColumn>(
            batch, col_ind_, [&](const auto& value, size_t, size_t real_ind) {
                result_data[real_ind] = Timestamp::ExtractMinute(value);
            });

        return std::make_shared<Int32Column>(std::move(result_data));
    }

private:
    size_t col_ind_;
};

class TimeTruncFunction : public ScalarFunction {
public:
    TimeTruncFunction(size_t col_ind, TimeUnitType part) : col_ind_(col_ind), part_(part) {
    }

    std::shared_ptr<Column> Evaluate(const RecordBatch& batch) override {
        size_t physical_size = batch.columns[0]->Size();
        std::vector<int64_t> result_data(physical_size);

        AggExpHelper::IterateColumnData<TimestampColumn>(
            batch, col_ind_, [&](const auto& value, size_t, size_t real_ind) {
                result_data[real_ind] = Timestamp::Truncate(value, part_);
            });

        return std::make_shared<TimestampColumn>(std::move(result_data));
    }

private:
    size_t col_ind_;
    TimeUnitType part_;
};

class LengthFunction : public ScalarFunction {
public:
    explicit LengthFunction(size_t col_ind) : col_ind_(col_ind) {
    }

    std::shared_ptr<Column> Evaluate(const RecordBatch& batch) override {
        size_t physical_size = batch.columns[0]->Size();
        std::vector<int64_t> result_data(physical_size);

        AggExpHelper::IterateColumnData<StringColumn>(
            batch, col_ind_, [&](const auto& value, size_t, size_t real_ind) {
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
        : type_(type), constant_(std::move(constant)) {
    }

    std::shared_ptr<Column> Evaluate(const RecordBatch& batch) override {
        size_t physical_size = batch.columns.empty() ? batch.num_rows : batch.columns[0]->Size();

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
        : col_ind_(col_ind), constant_(std::move(constant)) {
    }

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

template <typename ColumnClass, typename ConstantType>
class CaseWhenFunction : public ScalarFunction {
    using ContainerType = ColumnClass::ContainerType;
    using ReturnType = std::decay_t<decltype(std::declval<ContainerType>()[0])>;
    using BuilderType = BuilderTypeTrait<ColumnClass>::Type;

    using ResolvedBranch =
        std::variant<size_t, ConstantType>;  // либо индекс колонки, либо константа
    using ViewType = ViewVariant<ContainerType, ReturnType>;

public:
    CaseWhenFunction(std::unique_ptr<FilterFunction> cond_func, ResolvedBranch true_branch,
                     ResolvedBranch false_branch, ColumnType type)
        : cond_func_(std::move(cond_func)),
          true_branch_(std::move(true_branch)),
          false_branch_(std::move(false_branch)),
          type_(type) {
    }

    std::shared_ptr<Column> Evaluate(const RecordBatch& batch) override {
        std::vector<size_t> true_indices = cond_func_->Evaluate(batch);

        auto build_view = [&](const ResolvedBranch& b) -> ViewType {
            if (std::holds_alternative<size_t>(b)) {
                size_t col_idx = std::get<size_t>(b);
                auto* col = static_cast<const ColumnClass*>(batch.columns[col_idx].get());
                return FlatColumnView<ContainerType, ReturnType>(&col->GetData());
            } else {
                return ConstColumnView<ReturnType>(std::get<ConstantType>(b));
            }
        };

        ViewType true_view = build_view(true_branch_);
        ViewType false_view = build_view(false_branch_);

        auto builder = ColumnFactory::MakeColumnBuilder(type_);
        auto* typed_builder = static_cast<BuilderType*>(builder.get());

        std::visit(
            [&](const auto& true_v, const auto& false_v) {
                size_t physical_size =
                    batch.columns.empty() ? batch.num_rows : batch.columns[0]->Size();
                size_t cur_ind = 0;
                for (size_t real_ind = 0; real_ind < physical_size; ++real_ind) {
                    if (cur_ind < true_indices.size() && true_indices[cur_ind] == real_ind) {
                        typed_builder->AddValue(true_v[real_ind]);
                        ++cur_ind;
                    } else {
                        typed_builder->AddValue(false_v[real_ind]);
                    }
                }
            },
            true_view, false_view);

        return builder->Finish();
    }

private:
    std::unique_ptr<FilterFunction> cond_func_;
    ResolvedBranch true_branch_;
    ResolvedBranch false_branch_;
    ColumnType type_;
};

class RegexpReplaceFunction : public ScalarFunction {
public:
    RegexpReplaceFunction(size_t col_ind, const std::string& pattern,
                          const std::string& replacement)
        : col_ind_(col_ind), regex_(pattern), replacement_(replacement) {
    }

    std::shared_ptr<Column> Evaluate(const RecordBatch& batch) override {
        size_t physical_size = batch.columns.empty() ? batch.num_rows : batch.columns[0]->Size();
        std::vector<std::string> temp_res(physical_size);

        AggExpHelper::IterateColumnData<StringColumn>(
            batch, col_ind_, [&](const auto& value, size_t, size_t real_ind) {
                std::string str(value);
                temp_res[real_ind] =
                    std::regex_replace(str, regex_, replacement_, std::regex_constants::format_sed);
            });

        StringColumnBuilder builder;
        for (size_t i = 0; i < physical_size; ++i) {
            builder.AddValue(temp_res[i]);
        }
        return builder.Finish();
    }

private:
    size_t col_ind_;
    std::regex regex_;
    std::string replacement_;
};
