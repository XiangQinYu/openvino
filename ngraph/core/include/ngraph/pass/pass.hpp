// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <list>
#include <memory>
#include <vector>

#include "ngraph/deprecated.hpp"
#include "ngraph/function.hpp"
#include "ngraph/node.hpp"
#include "ngraph/pass/pass_config.hpp"
#include "ngraph/util.hpp"
#include "openvino/pass/pass.hpp"

namespace ov {
namespace pass {

class Manager;

}
}  // namespace ov
namespace ngraph {
namespace pass {
using ov::pass::FunctionPass;
using ov::pass::FusionType;
using ov::pass::FusionTypeMask;
using ov::pass::Manager;
using ov::pass::PassBase;
using ov::pass::PassProperty;
using ov::pass::PassPropertyMask;
NGRAPH_DEPRECATED("This variable is deprecated and will be removed soon.")
const PassPropertyMask all_pass_property_off;

class NGRAPH_DEPRECATED("Use MatcherPass or FunctionPass instead.") NGRAPH_API NodePass : public PassBase {
public:
    NGRAPH_RTTI_DECLARATION;
    ~NodePass() override;
    virtual bool run_on_node(std::shared_ptr<ngraph::Node>) = 0;
};
}  // namespace pass
}  // namespace ngraph
