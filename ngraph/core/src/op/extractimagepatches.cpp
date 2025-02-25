// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "ngraph/op/extractimagepatches.hpp"

#include "itt.hpp"
#include "ngraph/attribute_visitor.hpp"

using namespace std;
using namespace ngraph;

// ExtractImagePatches v3

BWDCMP_RTTI_DEFINITION(op::v3::ExtractImagePatches);

op::v3::ExtractImagePatches::ExtractImagePatches(const Output<Node>& image,
                                                 const ov::Shape& sizes,
                                                 const Strides& strides,
                                                 const ov::Shape& rates,
                                                 const PadType& auto_pad)
    : Op({image}),
      m_patch_sizes(sizes),
      m_patch_movement_strides(strides),
      m_patch_selection_rates(rates),
      m_padding(auto_pad) {
    constructor_validate_and_infer_types();
}

void op::v3::ExtractImagePatches::validate_and_infer_types() {
    NGRAPH_OP_SCOPE(v3_ExtractImagePatches_validate_and_infer_types);
    const ov::PartialShape input_pshape = get_input_partial_shape(0);

    NODE_VALIDATION_CHECK(this, input_pshape.rank() == 4, "input tensor must be 4D tensor.");

    NODE_VALIDATION_CHECK(this,
                          m_patch_sizes.size() == 2,
                          "Attribute sizes should be in [size_rows, size_cols] format.");

    NODE_VALIDATION_CHECK(this,
                          m_patch_movement_strides.size() == 2,
                          "Attribute strides should be in [stride_rows, stride_cols] format.");

    NODE_VALIDATION_CHECK(this,
                          m_patch_movement_strides[0] > 0 && m_patch_movement_strides[1] > 0,
                          "Attribute strides should be strictly greater than zeros in values.");

    NODE_VALIDATION_CHECK(this,
                          m_patch_selection_rates.size() == 2,
                          "Attribute rates should be in [rate_rows, rate_cols] format.");

    NODE_VALIDATION_CHECK(this,
                          m_patch_selection_rates[0] > 0 && m_patch_selection_rates[1] > 0,
                          "Attribute rates should be strictly greater than zeros in values.");

    NODE_VALIDATION_CHECK(
        this,
        m_padding == PadType::VALID || m_padding == PadType::SAME_LOWER || m_padding == PadType::SAME_UPPER,
        "Attribute padding should be in either valid or same_lower or same_upper.");

    if (input_pshape[1].is_dynamic() || input_pshape[2].is_dynamic() || input_pshape[3].is_dynamic()) {
        set_input_is_relevant_to_shape(0);
        auto output_pshape = ov::PartialShape::dynamic(4);
        set_output_type(0, get_input_element_type(0), output_pshape);
    } else {
        int32_t input_depth = input_pshape[1].get_length();
        int32_t input_rows = input_pshape[2].get_length();
        int32_t input_cols = input_pshape[3].get_length();
        int32_t out_rows(0);
        int32_t out_cols(0);

        if (input_rows == 0 || input_cols == 0) {
            out_rows = 0;
            out_cols = 0;
        } else if (m_padding == PadType::VALID) {
            out_rows =
                (((input_rows) -
                  static_cast<int32_t>(m_patch_selection_rates[0]) * (static_cast<int32_t>(m_patch_sizes[0]) - 1) - 1) /
                 m_patch_movement_strides[0]) +
                1;
            out_cols =
                (((input_cols) -
                  static_cast<int32_t>(m_patch_selection_rates[1]) * (static_cast<int32_t>(m_patch_sizes[1]) - 1) - 1) /
                 m_patch_movement_strides[1]) +
                1;
        } else {
            out_rows = 1 + (((input_rows)-1) / m_patch_movement_strides[0]);
            out_cols = 1 + (((input_cols)-1) / m_patch_movement_strides[1]);
        }

        if (out_rows < 0)
            out_rows = 0;
        if (out_cols < 0)
            out_cols = 0;

        auto out_depth_cast =
            static_cast<ngraph::Dimension::value_type>(input_depth * m_patch_sizes[0] * m_patch_sizes[1]);
        auto out_rows_cast = static_cast<ngraph::Dimension::value_type>(out_rows);
        auto out_cols_cast = static_cast<ngraph::Dimension::value_type>(out_cols);

        ov::PartialShape output_pshape;
        if (input_pshape[0].is_dynamic()) {
            output_pshape = ov::PartialShape{input_pshape[0], out_depth_cast, out_rows_cast, out_cols_cast};
        } else {
            auto input_batch_cast = static_cast<ngraph::Dimension::value_type>(input_pshape[0].get_length());
            output_pshape = ov::PartialShape{input_batch_cast, out_depth_cast, out_rows_cast, out_cols_cast};
        }

        if (input_rows == 0 || input_cols == 0) {
            output_pshape = input_pshape;
        }

        set_output_type(0, get_input_element_type(0), output_pshape);
    }
}

bool op::v3::ExtractImagePatches::visit_attributes(AttributeVisitor& visitor) {
    NGRAPH_OP_SCOPE(v3_ExtractImagePatches_visit_attributes);
    visitor.on_attribute("sizes", m_patch_sizes);
    visitor.on_attribute("strides", m_patch_movement_strides);
    visitor.on_attribute("rates", m_patch_selection_rates);
    visitor.on_attribute("auto_pad", m_padding);
    return true;
}

shared_ptr<Node> op::v3::ExtractImagePatches::clone_with_new_inputs(const OutputVector& new_args) const {
    NGRAPH_OP_SCOPE(v3_ExtractImagePatches_clone_with_new_inputs);
    check_new_args_count(this, new_args);
    return make_shared<op::v3::ExtractImagePatches>(new_args.at(0),
                                                    m_patch_sizes,
                                                    m_patch_movement_strides,
                                                    m_patch_selection_rates,
                                                    m_padding);
}
