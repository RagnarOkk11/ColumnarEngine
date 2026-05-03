//
// Created by ragnarokk on 5/3/26.
//

#ifndef COLUMNAR_ENGINE_SCALAREXPRESSIONS_H
#define COLUMNAR_ENGINE_SCALAREXPRESSIONS_H

class ScalarExpression {
public:
    virtual ~ScalarExpression() = default;
    virtual std::shared_ptr<Column> Evaluate(const RecordBatch& batch) = 0;
};

class ExtractMinuteExpression : public ScalarExpression {
public:
    explicit ExtractMinuteExpression(size_t input_col_idx) : input_col_idx_(input_col_idx) {}

    std::shared_ptr<Column> Evaluate(const RecordBatch& batch) override {
        auto* input_col = static_cast<TimestampColumn*>(batch.columns[input_col_idx_].get());
        const auto& input_data = input_col->GetData();

        auto result_col = std::make_shared<Int32Column>(); // Минуты отлично влезут в Int32

        // Оптимизация: учитываем selection_vector, если он есть (после Filter)
        if (batch.selection_vector) {
            for (size_t i = 0; i < batch.num_rows; ++i) {
                size_t actual_idx = (*batch.selection_vector)[i];
                result_col->Add(TimestampColumn::ExtractMinute(input_data[actual_idx]));
            }
        } else {
            for (size_t i = 0; i < input_col->Size(); ++i) {
                result_col->Add(TimestampColumn::ExtractMinute(input_data[i]));
            }
        }
        return result_col;
    }

private:
    size_t input_col_idx_;
};

#endif  // COLUMNAR_ENGINE_SCALAREXPRESSIONS_H
