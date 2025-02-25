// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "model.hpp"

#include <frontend_manager/frontend_exceptions.hpp>
#include <fstream>
#include <openvino/opsets/opset7.hpp>
#include <queue>
#include <tensorflow_frontend/graph_iterator.hpp>

#include "graph_iterator_proto.hpp"
#include "ngraph_conversions.hpp"
#include "node_context.hpp"
#include "place.hpp"
#include "utils.hpp"

using namespace google;

namespace ov {
namespace frontend {
namespace tf {
void extract_operation_name_and_port(const std::string& port_name,
                                     std::string& operation_name,
                                     size_t& port_index,
                                     std::string& port_type) {
    constexpr char delimeter[] = ":";
    auto pos = port_name.find(delimeter);
    if (pos == std::string::npos) {
        operation_name = port_name;
        port_type = "none";
        port_index = 0;
        return;
    }

    FRONT_END_GENERAL_CHECK((0 < pos) && (pos + 1 < port_name.length()), "Incorrect port name specified: " + port_name);

    auto left_part = port_name.substr(0, pos);
    auto right_part = port_name.substr(pos + 1, port_name.length() - pos);

    if (left_part.find_first_not_of("0123456789") == std::string::npos) {
        port_type = "in";
        operation_name = right_part;
        port_index = std::atoi(left_part.c_str());
    } else if (right_part.find_first_not_of("0123456789") == std::string::npos) {
        port_type = "out";
        operation_name = left_part;
        port_index = std::atoi(right_part.c_str());
    } else {
        FRONT_END_GENERAL_CHECK(false, "Incorrect port name specified: " + port_name);
    }
}
}  // namespace tf
class InputModelTF::InputModelTFImpl {
public:
    InputModelTFImpl(const GraphIterator::Ptr& graph_iterator, const ngraph::frontend::InputModel& input_model);
    std::vector<ngraph::frontend::Place::Ptr> getInputs() const;
    std::vector<ngraph::frontend::Place::Ptr> getOutputs() const;
    ngraph::frontend::Place::Ptr getPlaceByTensorName(const std::string& tensorName) const;
    void overrideAllOutputs(const std::vector<ngraph::frontend::Place::Ptr>& outputs);
    void overrideAllInputs(const std::vector<ngraph::frontend::Place::Ptr>& inputs);
    void extractSubgraph(const std::vector<ngraph::frontend::Place::Ptr>& inputs,
                         const std::vector<ngraph::frontend::Place::Ptr>& outputs);
    void setPartialShape(ngraph::frontend::Place::Ptr place, const ov::PartialShape&);
    ov::PartialShape getPartialShape(ngraph::frontend::Place::Ptr place) const;
    void setElementType(ngraph::frontend::Place::Ptr place, const ov::element::Type&);
    void setTensorValue(ngraph::frontend::Place::Ptr place, const void* value);

    std::vector<std::shared_ptr<OpPlaceTF>> get_op_places() const;
    std::map<std::string, std::shared_ptr<TensorPlaceTF>> get_tensor_places() const {
        return m_tensor_places;
    }
    std::map<std::string, Output<Node>> get_tensor_values() const {
        return m_tensor_values;
    };

private:
    void loadPlaces();
    std::vector<std::shared_ptr<OpPlaceTF>> determine_cut_nodes() const;

    std::vector<std::shared_ptr<OpPlaceTF>> m_op_places;
    std::map<std::string, std::shared_ptr<OpPlaceTF>> m_op_places_map;
    mutable std::map<std::string, std::shared_ptr<TensorPlaceTF>> m_tensor_places;
    std::vector<ngraph::frontend::Place::Ptr> m_inputs;
    std::vector<ngraph::frontend::Place::Ptr> m_outputs;
    std::map<std::string, Output<Node>> m_tensor_values;

    std::shared_ptr<GraphIterator> m_graph_iterator;
    const ngraph::frontend::InputModel& m_input_model;

    // shows if some nodes might be deleted from graph
    bool m_graph_changed = false;
};

void InputModelTF::InputModelTFImpl::loadPlaces() {
    std::set<std::string> all_op_names;
    std::set<std::string> op_names_with_consumers;

    m_inputs.clear();
    for (; !m_graph_iterator->is_end(); m_graph_iterator->next()) {
        auto node_decoder = m_graph_iterator->get_decoder();
        auto op_name = node_decoder->get_op_name();
        auto op_type = node_decoder->get_op_type();
        auto op_place = std::make_shared<OpPlaceTF>(m_input_model, node_decoder);
        all_op_names.insert(op_name);
        m_op_places.push_back(op_place);
        m_op_places_map[op_name] = op_place;
        if (op_type == "Placeholder") {
            auto pshape = std::dynamic_pointer_cast<VariantWrapper<ov::PartialShape>>(
                node_decoder->get_attribute("shape", VariantWrapper<ov::PartialShape>::get_type_info_static()));
            auto type = std::dynamic_pointer_cast<VariantWrapper<ov::element::Type>>(
                node_decoder->get_attribute("dtype", VariantWrapper<ov::element::Type>::get_type_info_static()));
            std::vector<std::string> names = {op_name};
            auto tensor_place = std::make_shared<TensorPlaceTF>(m_input_model, pshape->get(), type->get(), names);
            m_tensor_places[op_name] = tensor_place;
            m_inputs.push_back(tensor_place);
        }
        for (size_t input_port_idx = 0; input_port_idx < node_decoder->get_input_size(); ++input_port_idx) {
            std::string producer_op_name;
            size_t producer_output_port_idx;
            try {
                node_decoder->get_input_node(input_port_idx, producer_op_name, producer_output_port_idx);
                op_names_with_consumers.insert(producer_op_name);
            } catch (const std::exception& e) {
                FRONT_END_THROW("[ ERROR ] Exception happened when preparing input " + std::to_string(input_port_idx) +
                                " for op '" + node_decoder->get_op_name() + "', expected input name: '" +
                                producer_op_name +
                                "', expected input port index: " + std::to_string(producer_output_port_idx));
            }
        }
    }
    std::set<std::string> op_names_without_consumers;
    std::set_difference(all_op_names.begin(),
                        all_op_names.end(),
                        op_names_with_consumers.begin(),
                        op_names_with_consumers.end(),
                        std::inserter(op_names_without_consumers, op_names_without_consumers.begin()));
    m_graph_iterator->reset();

    m_outputs.clear();
    for (auto& output_name : op_names_without_consumers) {
        std::vector<std::string> output_names = {output_name};
        auto output_place =
            std::make_shared<TensorPlaceTF>(m_input_model, ov::PartialShape({}), ov::element::undefined, output_names);
        m_tensor_places[output_name] = output_place;
        m_outputs.push_back(output_place);
    }
}

std::vector<std::shared_ptr<OpPlaceTF>> InputModelTF::InputModelTFImpl::get_op_places() const {
    if (m_graph_changed) {
        return determine_cut_nodes();
    }
    return m_op_places;
}

std::vector<std::shared_ptr<OpPlaceTF>> InputModelTF::InputModelTFImpl::determine_cut_nodes() const {
    std::queue<std::shared_ptr<DecoderBase>> decoders_queue;
    std::unordered_set<std::string> visited;
    std::vector<std::shared_ptr<OpPlaceTF>> new_ops;
    for (const auto& output_place : m_outputs) {
        FRONT_END_GENERAL_CHECK(output_place->get_names().size() > 0, "TensorPlace must have at least one name.");
        auto output_place_name = output_place->get_names()[0];
        std::string operation_name;
        size_t port_idx;
        std::string port_type;
        tf::extract_operation_name_and_port(output_place_name, operation_name, port_idx, port_type);
        if (!visited.count(operation_name)) {
            visited.insert(operation_name);
            FRONT_END_GENERAL_CHECK(m_op_places_map.count(operation_name),
                                    "Custom specified output is incorrect: " + output_place_name);
            auto output_operation_place = m_op_places_map.at(operation_name);
            FRONT_END_GENERAL_CHECK(output_operation_place,
                                    "There is not operation place in the map: " + operation_name);
            new_ops.push_back(output_operation_place);
            decoders_queue.push(output_operation_place->get_decoder());
        }
    }
    while (!decoders_queue.empty()) {
        auto operation_decoder = decoders_queue.front();
        decoders_queue.pop();
        auto current_operation_name = operation_decoder->get_op_name();
        for (size_t input_port_idx = 0; input_port_idx < operation_decoder->get_input_size(); ++input_port_idx) {
            std::string producer_name;
            size_t producer_output_port_idx;
            try {
                operation_decoder->get_input_node(input_port_idx, producer_name, producer_output_port_idx);
            } catch (const std::exception& e) {
                FRONT_END_THROW("[ ERROR ] Exception happened when preparing input " + std::to_string(input_port_idx) +
                                " for op '" + operation_decoder->get_op_name() + "', expected input name: '" +
                                producer_name +
                                "', expected input port index: " + std::to_string(producer_output_port_idx) + '\n');
            }

            // TODO: re-implement the logic below using Place graph structure (with OpPlace, In/OutPortPlace
            // connections) and based on check if Place->is_input() decide to leave a node or not

            // is_input is a flag to leave producer operation node or not.
            // this producing node is not left if consumer is pruned by its input port,
            // the producer node is pruned by its output port or the producer becomes new input
            // 1. check if the current node is pruned by its input port
            bool is_input = false;
            std::string input_port_name = std::to_string(input_port_idx) + ":" + current_operation_name;
            if (m_tensor_places.find(input_port_name) != m_tensor_places.end()) {
                const auto& tensor_place = m_tensor_places[input_port_name];
                is_input = is_input || (tensor_place->is_input() ? true : false);
            }

            // 2. check if the producer node is pruned by its output port
            std::string output_port_name = producer_name + ":" + std::to_string(producer_output_port_idx);
            if (m_tensor_places.find(output_port_name) != m_tensor_places.end()) {
                const auto& tensor_place = m_tensor_places[output_port_name];
                is_input = is_input || (tensor_place->is_input() ? true : false);
            }

            // 3. check if the current node is an input
            FRONT_END_GENERAL_CHECK(m_op_places_map.count(producer_name),
                                    "There is no operation node with name: " + producer_name);
            const auto& producer_operation_place = m_op_places_map.at(producer_name);
            if (m_tensor_places.find(producer_name) != m_tensor_places.end()) {
                const auto& tensor_place = m_tensor_places[producer_name];
                is_input |= (tensor_place->is_input() ? true : false);
            }

            if (!is_input && !visited.count(producer_name)) {
                visited.insert(producer_name);
                new_ops.push_back(producer_operation_place);
                decoders_queue.push(producer_operation_place->get_decoder());
            }
        }
    }
    std::reverse(new_ops.begin(), new_ops.end());
    return new_ops;
}

InputModelTF::InputModelTFImpl::InputModelTFImpl(const GraphIterator::Ptr& graph_iterator,
                                                 const ngraph::frontend::InputModel& input_model)
    : m_input_model(input_model),
      m_graph_iterator(graph_iterator) {
    FRONT_END_GENERAL_CHECK(m_graph_iterator, "Null pointer specified for GraphIterator");
    loadPlaces();
}

std::vector<ngraph::frontend::Place::Ptr> InputModelTF::InputModelTFImpl::getInputs() const {
    return m_inputs;
}

std::vector<ngraph::frontend::Place::Ptr> InputModelTF::InputModelTFImpl::getOutputs() const {
    return m_outputs;
}

ngraph::frontend::Place::Ptr InputModelTF::InputModelTFImpl::getPlaceByTensorName(const std::string& tensorName) const {
    if (m_tensor_places.find(tensorName) != m_tensor_places.end())
        return m_tensor_places.at(tensorName);

    // check that operation node exists for which this place is specified
    std::string operation_name;
    size_t port_idx;
    std::string port_type;
    tf::extract_operation_name_and_port(tensorName, operation_name, port_idx, port_type);
    if (m_op_places_map.find(operation_name) != m_op_places_map.end()) {
        std::vector<std::string> names = {tensorName};
        auto m_var_place =
            std::make_shared<TensorPlaceTF>(m_input_model, ov::PartialShape(), ov::element::undefined, names);
        m_tensor_places[tensorName] = m_var_place;
        return m_var_place;
    }

    return nullptr;
}

std::shared_ptr<TensorPlaceTF> castToTensorPlace(const ngraph::frontend::Place::Ptr& place) {
    if (auto var_place = std::dynamic_pointer_cast<TensorPlaceTF>(place)) {
        return var_place;
    } else if (auto in_port_place = std::dynamic_pointer_cast<InPortPlaceTF>(place)) {
        return in_port_place->get_source_tensor_tf();
    } else if (auto out_port_place = std::dynamic_pointer_cast<OutPortPlaceTF>(place)) {
        return out_port_place->get_target_tensor_tf();
    }
    FRONT_END_GENERAL_CHECK(false, "Cannot cast this Place to TensorPlaceTF.");
}

void InputModelTF::InputModelTFImpl::overrideAllInputs(const std::vector<ngraph::frontend::Place::Ptr>& inputs) {
    m_graph_changed = true;
    m_inputs.clear();
    for (const auto& input_place : inputs) {
        m_inputs.push_back(castToTensorPlace(input_place));
    }
}

void InputModelTF::InputModelTFImpl::overrideAllOutputs(const std::vector<ngraph::frontend::Place::Ptr>& outputs) {
    m_graph_changed = true;
    m_outputs.clear();
    for (const auto& output_place : outputs) {
        m_outputs.push_back(castToTensorPlace(output_place));
    }
}

void InputModelTF::InputModelTFImpl::extractSubgraph(const std::vector<ngraph::frontend::Place::Ptr>& inputs,
                                                     const std::vector<ngraph::frontend::Place::Ptr>& outputs) {
    m_graph_changed = true;
    overrideAllInputs(inputs);
    overrideAllOutputs(outputs);
}

void InputModelTF::InputModelTFImpl::setPartialShape(ngraph::frontend::Place::Ptr place,
                                                     const ov::PartialShape& p_shape) {
    castToTensorPlace(place)->set_partial_shape(p_shape);
}

ov::PartialShape InputModelTF::InputModelTFImpl::getPartialShape(ngraph::frontend::Place::Ptr place) const {
    return castToTensorPlace(place)->get_partial_shape();
}

void InputModelTF::InputModelTFImpl::setElementType(ngraph::frontend::Place::Ptr place, const ov::element::Type& type) {
    castToTensorPlace(place)->set_element_type(type);
}

void InputModelTF::InputModelTFImpl::setTensorValue(ngraph::frontend::Place::Ptr place, const void* value) {
    m_graph_changed = true;
    auto tensor_place = castToTensorPlace(place);
    auto p_shape = tensor_place->get_partial_shape();
    auto type = tensor_place->get_element_type();
    auto constant = opset7::Constant::create(type, p_shape.to_shape(), value);
    auto name = tensor_place->get_names()[0];
    constant->set_friendly_name(name);
    m_tensor_values[name] = constant;
}

InputModelTF::InputModelTF(const GraphIterator::Ptr& graph_iterator)
    : _impl{std::make_shared<InputModelTFImpl>(graph_iterator, *this)} {}

std::vector<std::shared_ptr<OpPlaceTF>> InputModelTF::get_op_places() const {
    return _impl->get_op_places();
}

std::map<std::string, std::shared_ptr<TensorPlaceTF>> InputModelTF::get_tensor_places() const {
    return _impl->get_tensor_places();
}

std::map<std::string, Output<Node>> InputModelTF::get_tensor_values() const {
    return _impl->get_tensor_values();
}

std::vector<ngraph::frontend::Place::Ptr> InputModelTF::get_inputs() const {
    return _impl->getInputs();
}

std::vector<ngraph::frontend::Place::Ptr> InputModelTF::get_outputs() const {
    return _impl->getOutputs();
}

ngraph::frontend::Place::Ptr InputModelTF::get_place_by_tensor_name(const std::string& tensorName) const {
    return _impl->getPlaceByTensorName(tensorName);
}

void InputModelTF::override_all_outputs(const std::vector<ngraph::frontend::Place::Ptr>& outputs) {
    _impl->overrideAllOutputs(outputs);
}

void InputModelTF::override_all_inputs(const std::vector<ngraph::frontend::Place::Ptr>& inputs) {
    _impl->overrideAllInputs(inputs);
}

void InputModelTF::extract_subgraph(const std::vector<ngraph::frontend::Place::Ptr>& inputs,
                                    const std::vector<ngraph::frontend::Place::Ptr>& outputs) {
    _impl->extractSubgraph(inputs, outputs);
}

void InputModelTF::set_partial_shape(ngraph::frontend::Place::Ptr place, const ov::PartialShape& p_shape) {
    _impl->setPartialShape(place, p_shape);
}

ov::PartialShape InputModelTF::get_partial_shape(ngraph::frontend::Place::Ptr place) const {
    return _impl->getPartialShape(place);
}

void InputModelTF::set_element_type(ngraph::frontend::Place::Ptr place, const ov::element::Type& type) {
    _impl->setElementType(place, type);
}

void InputModelTF::set_tensor_value(ngraph::frontend::Place::Ptr place, const void* value) {
    _impl->setTensorValue(place, value);
}
}  // namespace frontend
}  // namespace ov
