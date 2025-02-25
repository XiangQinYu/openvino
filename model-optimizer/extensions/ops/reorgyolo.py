# Copyright (C) 2018-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

import numpy as np

from mo.graph.graph import Node, Graph
from mo.ops.op import Op, PermuteAttrs
from mo.utils.error import Error


class ReorgYoloOp(Op):
    op = 'ReorgYolo'

    def __init__(self, graph: Graph, attrs: dict):
        mandatory_props = {
            'type': self.op,
            'op': self.op,
            'version': 'opset2',
            'infer': ReorgYoloOp.reorgyolo_infer
        }
        super().__init__(graph, mandatory_props, attrs)

    def supported_attrs(self):
        return [
            'stride'
        ]

    @staticmethod
    def reorgyolo_infer(node: Node):
        input_shape = node.in_node(0).shape
        if input_shape is None:
            raise Error('Input shape for operation "{}" is None'.format(node.soft_get('name', node.id)))

        stride = node.stride

        output_shape = input_shape.copy()
        output_shape[node.batch_dims] = input_shape[node.batch_dims]  # pylint: disable=unsupported-assignment-operation
        output_shape[node.channel_dims] = input_shape[node.channel_dims] * stride ** 2  # pylint: disable=unsupported-assignment-operation
        # Round as in caffe
        output_shape[node.spatial_dims] = np.ma.round(input_shape[node.spatial_dims] / stride)  # pylint: disable=unsupported-assignment-operation

        node.out_port(0).data.set_shape(output_shape)
        PermuteAttrs.create_permute_attrs(node, attrs=[('channel_dims', 'input:0'), ('spatial_dims', 'input:0')])
