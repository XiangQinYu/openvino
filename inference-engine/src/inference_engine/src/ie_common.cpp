// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "ie_common.h"

#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "exec_graph_info.hpp"
#include "ie_blob.h"
#include "ie_extension.h"
#include "ie_iextension.h"
#include "ie_parameter.hpp"
#include "ngraph/opsets/opset.hpp"

namespace ExecGraphInfoSerialization {
//
// exec_graph_info.hpp
//
constexpr ngraph::NodeTypeInfo ExecutionNode::type_info;

const ngraph::NodeTypeInfo& ExecutionNode::get_type_info() const {
    return type_info;
}
}  // namespace ExecGraphInfoSerialization

namespace InferenceEngine {

//
// ie_iextension.h
//
ILayerImpl::~ILayerImpl() {}
ILayerExecImpl::~ILayerExecImpl() {}
std::map<std::string, ngraph::OpSet> IExtension::getOpSets() {
    return {};
}

//
// ie_extension.h
//
std::map<std::string, ngraph::OpSet> Extension::getOpSets() {
    return actual->getOpSets();
}
namespace details {

void Rethrow() {
    try {
        throw;
    } catch (const GeneralError& e) {
        throw e;
    } catch (const NotImplemented& e) {
        throw e;
    } catch (const NetworkNotLoaded& e) {
        throw e;
    } catch (const ParameterMismatch& e) {
        throw e;
    } catch (const NotFound& e) {
        throw e;
    } catch (const OutOfBounds& e) {
        throw e;
    } catch (const Unexpected& e) {
        throw e;
    } catch (const RequestBusy& e) {
        throw e;
    } catch (const ResultNotReady& e) {
        throw e;
    } catch (const NotAllocated& e) {
        throw e;
    } catch (const InferNotStarted& e) {
        throw e;
    } catch (const NetworkNotRead& e) {
        throw e;
    } catch (const InferCancelled& e) {
        throw e;
    } catch (const std::exception& e) {
        IE_THROW() << e.what();
    } catch (...) {
        IE_THROW(Unexpected);
    }
}

IE_SUPPRESS_DEPRECATED_START

StatusCode InferenceEngineException::getStatus() const {
    if (dynamic_cast<const GeneralError*>(this) != nullptr) {
        return GENERAL_ERROR;
    } else if (dynamic_cast<const NotImplemented*>(this) != nullptr) {
        return NOT_IMPLEMENTED;
    } else if (dynamic_cast<const NetworkNotLoaded*>(this) != nullptr) {
        return NETWORK_NOT_LOADED;
    } else if (dynamic_cast<const ParameterMismatch*>(this) != nullptr) {
        return PARAMETER_MISMATCH;
    } else if (dynamic_cast<const NotFound*>(this) != nullptr) {
        return NOT_FOUND;
    } else if (dynamic_cast<const OutOfBounds*>(this) != nullptr) {
        return OUT_OF_BOUNDS;
    } else if (dynamic_cast<const Unexpected*>(this) != nullptr) {
        return UNEXPECTED;
    } else if (dynamic_cast<const RequestBusy*>(this) != nullptr) {
        return REQUEST_BUSY;
    } else if (dynamic_cast<const ResultNotReady*>(this) != nullptr) {
        return RESULT_NOT_READY;
    } else if (dynamic_cast<const NotAllocated*>(this) != nullptr) {
        return NOT_ALLOCATED;
    } else if (dynamic_cast<const InferNotStarted*>(this) != nullptr) {
        return INFER_NOT_STARTED;
    } else if (dynamic_cast<const NetworkNotRead*>(this) != nullptr) {
        return NETWORK_NOT_READ;
    } else if (dynamic_cast<const InferCancelled*>(this) != nullptr) {
        return INFER_CANCELLED;
    } else {
        assert(!"Unreachable");
        return OK;
    }
}
}  // namespace details
IE_SUPPRESS_DEPRECATED_END

}  // namespace InferenceEngine

namespace ov {

namespace runtime {

//
// openvino/runtime/parameter.hpp
//

Parameter::~Parameter() {
    clear();
}

#ifdef __ANDROID__
Parameter::Any::~Any() {}

template struct Parameter::RealData<InferenceEngine::Blob::Ptr>;
template struct Parameter::RealData<int>;
template struct Parameter::RealData<bool>;
template struct Parameter::RealData<float>;
template struct Parameter::RealData<double>;
template struct Parameter::RealData<uint32_t>;
template struct Parameter::RealData<std::string>;
template struct Parameter::RealData<unsigned long>;
template struct Parameter::RealData<std::vector<int>>;
template struct Parameter::RealData<std::vector<std::string>>;
template struct Parameter::RealData<std::vector<unsigned long>>;
template struct Parameter::RealData<std::tuple<unsigned int, unsigned int>>;
template struct Parameter::RealData<std::tuple<unsigned int, unsigned int, unsigned int>>;
#endif

}  // namespace runtime

}  // namespace ov
