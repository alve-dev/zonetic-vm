#pragma once

#include "common.hpp"

namespace zonvm {
    struct Value {
        int32_t number;
        Value() : number(0.0) {}
        Value(int32_t val) : number(val) {}
    };
}