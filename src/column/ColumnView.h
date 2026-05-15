#pragma once

#include <variant>

template <typename ContainerType, typename ReturnType>
struct FlatColumnView {
    const ContainerType* container;

    explicit FlatColumnView(const ContainerType* c) : container(c) {}

    inline ReturnType operator[](size_t i) const {
        return (*container)[i];
    }
};

template <typename ReturnType>
struct ConstColumnView {
    ReturnType value;

    explicit ConstColumnView(ReturnType val) : value(std::move(val)) {}

    inline ReturnType operator[](size_t) const {
        return value;
    }
};

template <typename ContainerType, typename ReturnType>
using ViewVariant = std::variant<FlatColumnView<ContainerType, ReturnType>, ConstColumnView<ReturnType>>;