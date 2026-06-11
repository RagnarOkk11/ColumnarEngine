#pragma once

#include "execution/expressions/AggExpHelper.h"
#include "execution/OperatorsBase.h"

#include "column/Column.h"
#include "column/CharColumn.h"
#include "column/NumericColumn.h"
#include "column/StringColumn.h"
#include "column/TemporalColumn.h"
#include "column/ColumnFactory.h"

#include "absl/container/flat_hash_set.h"

#include <memory>
#include <type_traits>
#include <vector>

class AggregationFunction {
public:
    virtual ~AggregationFunction() = default;
    AggregationFunction(ColumnType column_type, size_t num_threads)
        : column_type_(column_type), num_threads_(num_threads) {
    }

    std::shared_ptr<Column> Finalize() {
        auto builder = ColumnFactory::MakeColumnBuilder(column_type_);
        WriteResult(builder);
        return builder->Finish();
    }

protected:
    ColumnType column_type_;
    size_t num_threads_;

    virtual void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) = 0;
};

class GlobalAggregationFunction : public AggregationFunction {
public:
    GlobalAggregationFunction(ColumnType column_type, size_t num_threads)
        : AggregationFunction(column_type, num_threads) {
    }

    virtual void Update(const RecordBatch& batch, size_t thread_id) = 0;
};

class GroupedAggregationFunction : public AggregationFunction {
public:
    GroupedAggregationFunction(ColumnType column_type, size_t num_threads)
        : AggregationFunction(column_type, num_threads) {
    }

    virtual void Resize(size_t num_groups, size_t thread_id) = 0;
    virtual void Update(const RecordBatch& batch, const std::vector<uint32_t>& group_ids,
                        size_t thread_id) = 0;
    virtual void CombineState(size_t src_thread, uint32_t src_gid, uint32_t global_gid) = 0;
};

struct SumOperation {
    template <typename ResultType, typename ValueType>
    static void Apply(ResultType& result, const ValueType& value) {
        result += value;
    }
};

struct MaxOperation {
    template <typename ResultType, typename ValueType>
    static void Apply(ResultType& result, const ValueType& value) {
        if (value > result) {
            result = value;
        }
    }
};

struct MinOperation {
    template <typename ResultType, typename ValueType>
    static inline void Apply(ResultType& result, const ValueType& value) {
        if (value < result) {
            result = value;
        }
    }
};

template <typename InputColumn, typename OutputColumn, typename Operation>
class TypedGlobalAggregationFunction : public GlobalAggregationFunction {
    using StateType = OutputColumn::ValueType;
    using OutputBuilder = BuilderTypeTrait<OutputColumn>::Type;

    struct alignas(64) ThreadState {
        StateType value{};
        bool has_data = false;
    };

public:
    TypedGlobalAggregationFunction(size_t column_index, ColumnType column_type, size_t num_threads)
        : GlobalAggregationFunction(column_type, num_threads),
          column_index_(column_index),
          thread_states_(num_threads) {
    }

    void Update(const RecordBatch& batch, size_t thread_id) override {
        ThreadState& local_state = thread_states_[thread_id];

        AggExpHelper::IterateColumnData<InputColumn>(
            batch, column_index_, [&](const auto& value, size_t, size_t) {
                if (!local_state.has_data) {
                    local_state.value = value;
                    local_state.has_data = true;
                } else {
                    Operation::Apply(local_state.value, value);
                }
            });
    }

protected:
    void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) override {
        StateType result{};
        bool has_data = false;

        for (size_t i = 0; i < thread_states_.size(); i++) {
            if (has_data) {
                if (thread_states_[i].has_data) {
                    Operation::Apply(result, thread_states_[i].value);
                }
            } else if (thread_states_[i].has_data) {
                has_data = true;
                result = thread_states_[i].value;
            }
        }

        auto* typed = static_cast<OutputBuilder*>(builder.get());
        if (has_data) {
            typed->AddValue(result);
        } else {
            if constexpr (requires { Operation::template GetInitValue<StateType>(); }) {
                typed->AddValue(Operation::template GetInitValue<StateType>());
            } else {
                typed->AddValue(StateType{});
            }
        }
    }

private:
    const size_t column_index_;
    std::vector<ThreadState> thread_states_;
};

template <typename InputColumn, typename OutputColumn, typename Operation>
class TypedGroupedAggregationFunction : public GroupedAggregationFunction {
    using StateType = OutputColumn::ValueType;
    using OutputBuilder = BuilderTypeTrait<OutputColumn>::Type;

public:
    TypedGroupedAggregationFunction(size_t column_index, ColumnType column_type, size_t num_threads)
        : GroupedAggregationFunction(column_type, num_threads),
          column_index_(column_index),
          thread_states_(num_threads) {
    }

    void Resize(size_t num_groups, size_t thread_id) override {
        auto& local = thread_states_[thread_id];
        if (num_groups > local.states.size()) {
            local.states.resize(num_groups);
            local.has_data.resize(num_groups, 0);
        }
    }

    void Update(const RecordBatch& batch, const std::vector<uint32_t>& group_ids,
                size_t thread_id) override {
        auto& local = thread_states_[thread_id];
        AggExpHelper::IterateColumnData<InputColumn>(
            batch, column_index_, [&](const auto& value, size_t row_idx, size_t) {
                uint32_t gid = group_ids[row_idx];
                if (!local.has_data[gid]) {
                    local.states[gid] = value;
                    local.has_data[gid] = 1;
                } else {
                    Operation::Apply(local.states[gid], value);
                }
            });
    }

    void CombineState(size_t src_thread, uint32_t src_gid, uint32_t global_gid) override {
        auto& src = thread_states_[src_thread];
        auto& global = thread_states_[0];

        if (src.has_data[src_gid]) {
            if (!global.has_data[global_gid]) {
                global.states[global_gid] = src.states[src_gid];
                global.has_data[global_gid] = 1;
            } else {
                Operation::Apply(global.states[global_gid], src.states[src_gid]);
            }
        }
    }

protected:
    void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) override {
        auto* typed = static_cast<OutputBuilder*>(builder.get());
        auto& global = thread_states_[0];
        for (const StateType& state : global.states) {
            typed->AddValue(state);
        }
    }

private:
    struct alignas(64) ThreadState {
        std::vector<StateType> states;
        std::vector<char> has_data;
    };

    size_t column_index_;
    std::vector<ThreadState> thread_states_;
};

template <typename OutputColumn>
class CountGlobalAggregationFunction : public GlobalAggregationFunction {
    using ValueType = OutputColumn::ValueType;
    using OutputBuilder = BuilderTypeTrait<OutputColumn>::Type;

    struct alignas(64) ThreadState {
        int64_t count = 0;
    };

public:
    CountGlobalAggregationFunction(ColumnType column_type, size_t num_threads)
        : GlobalAggregationFunction(column_type, num_threads), thread_states_(num_threads) {
    }

    void Update(const RecordBatch& batch, size_t thread_id) override {
        thread_states_[thread_id].count += batch.num_rows;
    }

protected:
    void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) override {
        int64_t total = 0;
        for (const auto& state : thread_states_) {
            total += state.count;
        }
        auto* typed = static_cast<OutputBuilder*>(builder.get());
        typed->AddValue(static_cast<ValueType>(total));
    }

private:
    std::vector<ThreadState> thread_states_;
};

template <typename OutputColumn>
class CountGroupedAggregationFunction : public GroupedAggregationFunction {
    using ValueType = OutputColumn::ValueType;
    using OutputBuilder = BuilderTypeTrait<OutputColumn>::Type;

public:
    CountGroupedAggregationFunction(ColumnType column_type, size_t num_threads)
        : GroupedAggregationFunction(column_type, num_threads), thread_states_(num_threads) {
    }

    void Resize(size_t num_groups, size_t thread_id) override {
        auto& local = thread_states_[thread_id];
        if (num_groups > local.counts.size()) {
            local.counts.resize(num_groups, 0);
        }
    }

    void Update(const RecordBatch& batch, const std::vector<uint32_t>& group_ids,
                size_t thread_id) override {
        auto& local = thread_states_[thread_id];
        for (size_t i = 0; i < batch.num_rows; ++i) {
            uint32_t gid = group_ids[i];
            ++local.counts[gid];
        }
    }

    void CombineState(size_t src_thread, uint32_t src_gid, uint32_t global_gid) override {
        ThreadState& src = thread_states_[src_thread];
        ThreadState& global = thread_states_[0];
        global.counts[global_gid] += src.counts[src_gid];
    }

protected:
    void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) override {
        auto* typed = static_cast<OutputBuilder*>(builder.get());
        auto& global = thread_states_[0];

        for (int64_t c : global.counts) {
            typed->AddValue(static_cast<ValueType>(c));
        }
    }

private:
    struct alignas(64) ThreadState {
        std::vector<int64_t> counts;
    };

    std::vector<ThreadState> thread_states_;
};

template <typename InputColumn, typename OutputColumn, typename StateColumn>
class AvgGlobalAggregationFunction : public GlobalAggregationFunction {
    using StateType = StateColumn::ValueType;
    using OutputType = OutputColumn::ValueType;
    using OutputBuilder = BuilderTypeTrait<OutputColumn>::Type;

    struct alignas(64) ThreadState {
        StateType sum = 0;
        int64_t count = 0;
    };

public:
    AvgGlobalAggregationFunction(size_t column_index, ColumnType column_type, size_t num_threads)
        : GlobalAggregationFunction(column_type, num_threads),
          column_index_(column_index),
          thread_states_(num_threads) {
    }

    void Update(const RecordBatch& batch, size_t thread_id) override {
        ThreadState& local = thread_states_[thread_id];
        AggExpHelper::IterateColumnData<InputColumn>(batch, column_index_,
                                                     [&](const auto& value, size_t, size_t) {
                                                         local.sum += value;
                                                         ++local.count;
                                                     });
    }

protected:
    void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) override {
        StateType total_sum = 0;
        int64_t total_count = 0;
        for (const auto& state : thread_states_) {
            total_sum += state.sum;
            total_count += state.count;
        }

        auto* typed = static_cast<OutputBuilder*>(builder.get());
        if (total_count > 0) {
            typed->AddValue(ComputeAvg(total_sum, total_count));
        } else {
            typed->AddValue(OutputType{});
        }
    }

private:
    static OutputType ComputeAvg(StateType sum, int64_t count) {
        if constexpr (std::is_integral_v<StateType>) {
            OutputType int_part = static_cast<OutputType>(sum / count);
            OutputType rem = static_cast<OutputType>(static_cast<long double>(sum % count) / count);
            return int_part + rem;
        } else {
            return static_cast<OutputType>(sum) / static_cast<OutputType>(count);
        }
    }

    size_t column_index_;
    std::vector<ThreadState> thread_states_;
};

template <typename InputColumn, typename OutputColumn, typename StateColumn>
class AvgGroupedAggregationFunction : public GroupedAggregationFunction {
    using StateType = StateColumn::ValueType;
    using OutputType = OutputColumn::ValueType;
    using OutputBuilder = BuilderTypeTrait<OutputColumn>::Type;

public:
    AvgGroupedAggregationFunction(size_t column_index, ColumnType column_type, size_t num_threads)
        : GroupedAggregationFunction(column_type, num_threads),
          column_index_(column_index),
          thread_states_(num_threads) {
    }

    void Resize(size_t num_groups, size_t thread_id) override {
        auto& local = thread_states_[thread_id];
        if (num_groups > local.states.size()) {
            local.states.resize(num_groups);
        }
    }

    void Update(const RecordBatch& batch, const std::vector<uint32_t>& group_ids,
                size_t thread_id) override {
        auto& local = thread_states_[thread_id];
        AggExpHelper::IterateColumnData<InputColumn>(
            batch, column_index_, [&](const auto& value, size_t row_idx, size_t) {
                uint32_t gid = group_ids[row_idx];
                local.states[gid].sum += value;
                ++local.states[gid].count;
            });
    }

    void CombineState(size_t src_thread, uint32_t src_gid, uint32_t global_gid) override {
        ThreadState& src = thread_states_[src_thread];
        ThreadState& global = thread_states_[0];
        global.states[global_gid].sum += src.states[src_gid].sum;
        global.states[global_gid].count += src.states[src_gid].count;
    }

protected:
    void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) override {
        auto* typed = static_cast<OutputBuilder*>(builder.get());
        auto& global = thread_states_[0];

        for (const auto& state : global.states) {
            if (state.count > 0) {
                typed->AddValue(ComputeAvg(state.sum, state.count));
            } else {
                typed->AddValue(OutputType{});
            }
        }
    }

private:
    static OutputType ComputeAvg(StateType sum, int64_t count) {
        if constexpr (std::is_integral_v<StateType>) {
            OutputType int_part = static_cast<OutputType>(sum / count);
            OutputType rem = static_cast<OutputType>(static_cast<long double>(sum % count) / count);
            return int_part + rem;
        } else {
            return static_cast<OutputType>(sum) / static_cast<OutputType>(count);
        }
    }

    struct alignas(64) ThreadState {
        struct State {
            StateType sum;
            int64_t count;
        };

        std::vector<State> states;
    };

    size_t column_index_;
    std::vector<ThreadState> thread_states_;
};

template <typename InputColumn, typename OutputColumn>
class DistinctCountGlobalAggregationFunction : public GlobalAggregationFunction {
    using InputValueType = InputColumn::ValueType;
    using OutputValueType = OutputColumn::ValueType;
    using OutputBuilder = BuilderTypeTrait<OutputColumn>::Type;

public:
    DistinctCountGlobalAggregationFunction(size_t column_index, ColumnType column_type,
                                           size_t num_threads)
        : GlobalAggregationFunction(column_type, num_threads),
          column_index_(column_index),
          thread_sets_(num_threads) {
    }

    void Update(const RecordBatch& batch, size_t thread_id) override {
        auto& local_set = thread_sets_[thread_id].set;
        AggExpHelper::IterateColumnData<InputColumn>(
            batch, column_index_,
            [&](const auto& value, size_t, size_t) { local_set.insert(InputValueType(value)); });
    }

protected:
    void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) override {
        auto& merged = thread_sets_[0].set;
        for (size_t i = 1; i < thread_sets_.size(); ++i) {
            for (auto& val : thread_sets_[i].set) {
                merged.insert(std::move(val));
            }
        }

        auto* typed = static_cast<OutputBuilder*>(builder.get());
        typed->AddValue(static_cast<OutputValueType>(merged.size()));
    }

private:
    struct alignas(64) Set {
        absl::flat_hash_set<InputValueType> set;
    };

    size_t column_index_;
    std::vector<Set> thread_sets_;
};

template <typename InputColumn, typename OutputColumn>
class DistinctCountGroupedAggregationFunction : public GroupedAggregationFunction {
    using InputValueType = InputColumn::ValueType;
    using OutputValueType = OutputColumn::ValueType;
    using OutputBuilder = BuilderTypeTrait<OutputColumn>::Type;

public:
    DistinctCountGroupedAggregationFunction(size_t column_index, ColumnType column_type,
                                            size_t num_threads)
        : GroupedAggregationFunction(column_type, num_threads),
          column_index_(column_index),
          thread_states_(num_threads) {
    }

    void Resize(size_t num_groups, size_t thread_id) override {
        auto& local = thread_states_[thread_id];
        if (num_groups > local.sets.size()) {
            local.sets.resize(num_groups);
        }
    }

    void Update(const RecordBatch& batch, const std::vector<uint32_t>& group_ids,
                size_t thread_id) override {
        auto& local = thread_states_[thread_id];
        AggExpHelper::IterateColumnData<InputColumn>(
            batch, column_index_, [&](const auto& value, size_t row_idx, size_t) {
                uint32_t gid = group_ids[row_idx];
                local.sets[gid].set.insert(InputValueType(value));
            });
    }

    void CombineState(size_t src_thread, uint32_t src_gid, uint32_t global_gid) override {
        ThreadState& src = thread_states_[src_thread];
        ThreadState& global = thread_states_[0];
        for (const auto& val : src.sets[src_gid].set) {
            global.sets[global_gid].set.insert(val);
        }
    }

protected:
    void WriteResult(const std::shared_ptr<ColumnBuilder>& builder) override {
        OutputBuilder* typed = static_cast<OutputBuilder*>(builder.get());
        ThreadState& global = thread_states_[0];

        for (const auto& wrapper : global.sets) {
            typed->AddValue(static_cast<OutputValueType>(wrapper.set.size()));
        }
    }

private:
    struct alignas(64) ThreadState {
        struct Set {
            absl::flat_hash_set<InputValueType> set;
        };

        std::vector<Set> sets;
    };

    size_t column_index_;
    std::vector<ThreadState> thread_states_;
};
