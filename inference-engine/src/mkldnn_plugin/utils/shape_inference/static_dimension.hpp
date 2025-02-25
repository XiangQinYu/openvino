// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <ostream>

namespace ov {
/// \brief Class representing a dimension, which must be static,
///        in a shape or shape-like object.
///
/// Provides similar API to the public Dimension class.
class StaticDimension {
public:
    using value_type = size_t;

    /// \brief Construct a static dimension.
    /// \param dimension Value of the dimension.
    StaticDimension(value_type dimension);

    /// \brief Construct a zero dimension
    StaticDimension() = default;

    bool operator==(const StaticDimension& dimension) const;
    bool operator!=(const StaticDimension& dimension) const;

    static bool is_static() { return true; }
    static bool is_dynamic() { return false; }

    value_type get_length() const;
    value_type get_min_length() const;
    value_type get_max_length() const;

    bool same_scheme(const StaticDimension& dim) const;
    bool compatible(const StaticDimension& d) const;
    static bool merge(StaticDimension& dst, const StaticDimension& d1, const StaticDimension& d2);
    static bool broadcast_merge(StaticDimension& dst, const StaticDimension& d1, const StaticDimension& d2);

    StaticDimension operator+(const StaticDimension& dim) const;
    StaticDimension operator-(const StaticDimension& dim) const;
    StaticDimension operator*(const StaticDimension& dim) const;
    StaticDimension operator&(const StaticDimension& dim) const;
    StaticDimension& operator+=(const StaticDimension& dim);
    StaticDimension& operator*=(const StaticDimension& dim);
    StaticDimension& operator&=(const StaticDimension& dim);

private:
    value_type m_dimension = 0;
};

std::ostream& operator<<(std::ostream& str, const StaticDimension& dimension);
}  // namespace ov
