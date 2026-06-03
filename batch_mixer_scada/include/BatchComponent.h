#pragma once
#include <concepts>
#include <string_view>

// C++20 Concept: any type satisfying this models a HAL component in the batch mixer.
template<typename T>
concept BatchComponent =
    requires(T t, double v) {
        { t.init() }     -> std::same_as<bool>;
        { t.selfTest() } -> std::same_as<bool>;
        { t.setpoint(v) } -> std::same_as<void>;
        { t.read() }     -> std::convertible_to<double>;
        { t.name() }     -> std::convertible_to<std::string_view>;
    };
