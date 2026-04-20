#pragma once

#include "common.hpp"

namespace zonvm {
    struct Value {
        double number;
        Value() : number(0.0) {}
        Value(double val) : number(val) {}
    };
}