// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <memory>

#include <ie_api.h>

#include "ngraph/op/op.hpp"

namespace ngraph {
namespace op {

class INFERENCE_ENGINE_API_CLASS(SeluIE) : public Op {
public:
    OPENVINO_OP("SeluIE", "legacy");
    BWDCMP_RTTI_DECLARATION;

    SeluIE(const Output<Node> & input,
           const float alpha,
           const float gamma);

    void validate_and_infer_types() override;

    std::shared_ptr<Node> clone_with_new_inputs(const OutputVector& new_args) const override;
    bool visit_attributes(AttributeVisitor& visitor) override;
    float gamma, alpha;
};

}  // namespace op
}  // namespace ngraph
