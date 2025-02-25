// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <transformations_visibility.hpp>

#include "ngraph/op/op.hpp"

namespace ngraph {
namespace snippets {
namespace op {

/**
 * @interface Nop
 * @brief Generated by Canonicalization and represents not-an-operation
 * @ingroup snippets
 */
class TRANSFORMATIONS_API Nop : public ngraph::op::Op {
public:
    OPENVINO_OP("Nop", "SnippetsOpset");

    Nop(const OutputVector& arguments, const OutputVector& results);
    Nop() = default;

    std::shared_ptr<Node> clone_with_new_inputs(const OutputVector& inputs) const override {
        return std::make_shared<Nop>();
    }
};

} // namespace op
} // namespace snippets
} // namespace ngraph