import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2' 
import tensorflow as tf
import numpy as np
import argparse
import json
import onnx
import networkx as nx
import copy
import matplotlib.pyplot as plt

class PBPredictor:
    def __init__(self, onnx_path, arch_path):
        super().__init__()
        
        with open(arch_path, 'r') as f:
          self.modules_order = json.load(f)

        self.model_name = onnx_path
        self.modules = []
        for module in self.modules_order:
          self.modules.append([module, True])
        self.graph = onnx.load(onnx_path).graph
        # get input shape
        self.input_shape = (self.graph.input[0].type.tensor_type.shape.dim[1].dim_value,
                            self.graph.input[0].type.tensor_type.shape.dim[2].dim_value,
                            self.graph.input[0].type.tensor_type.shape.dim[3].dim_value)


        self.network = self.onnxToNetworkx(self.graph)

        prelu_count = 0
        if 'ENet' in onnx_path:
          for node in self.network.nodes:
            if 'Transpose' in node and node in self.network.nodes:
              for successor in self.network.successors(node):
                self.network = nx.contracted_nodes(self.network, node, successor, self_loops=False)
              for successor in self.network.successors(node):
                # print(node, successor)
                self.network = nx.contracted_nodes(self.network, node, successor, self_loops=False)
              for successor in self.network.successors(node):
                # print(node, successor)
                self.network = nx.contracted_nodes(self.network, node, successor, self_loops=False)
              for successor in self.network.successors(node):
                # print(node, successor)
                self.network = nx.contracted_nodes(self.network, node, successor, self_loops=False)
              for successor in self.network.successors(node):
                output_node =  self.network.nodes[successor]['data'].output[0]
                # print(output_node)
                self.network = nx.contracted_nodes(self.network, node, successor, self_loops=False)
              for predecessor in self.network.predecessors(node):
                pred_data = self.network.nodes[predecessor]['data']
                if pred_data.op_type == 'Relu':
                  # print(node, predecessor)
                  self.network = nx.contracted_nodes(self.network, node, predecessor, self_loops=False)
              prelu_count += 1
              mapping = {node: 'PReLU_'+str(prelu_count)}
              self.network = nx.relabel_nodes(self.network, mapping)
              node_data = self.network.nodes[mapping[node]]['data']
              node_data.name = mapping[node]
              node_data.op_type = 'PReLU'
              node_data.output[0] = output_node

        nconv_count = 0
        dconv_count = 0
        tconv_count = 0
        prelu_count = 0
        add_count = 0
        relu_count = 0
        maxpool_count = 0
        bn_count = 0
        upsample_count = 0
        concat_count = 0
        for node in self.network.nodes:
          node_data = self.network.nodes[node]['data']
          # print(node_data.attribute[0])
          if node_data.op_type=='Conv':
            for attr in node_data.attribute:
              if attr.name == 'dilations':
                if attr.ints[0]==1 and attr.ints[1]==1:
                  nconv_count += 1
                else:
                  dconv_count += 1
          elif node_data.op_type=='ConvTranspose':
            tconv_count += 1
          elif node_data.op_type=='PReLU':
            prelu_count += 1
          elif node_data.op_type=='Add':
            add_count += 1
          elif node_data.op_type=='Relu':
            relu_count += 1
          elif node_data.op_type=='MaxPool':
            maxpool_count += 1
          elif node_data.op_type=='BatchNormalization':
            bn_count += 1
          elif node_data.op_type=='Upsample':
            upsample_count += 1
          elif node_data.op_type=='Concat':
            concat_count += 1

        self.reversed_network = nx.reverse(self.network)
        # read modules order of the architecture



        self.input_shapes = [self.get_node_shape(self.graph.input[i]) for i in range(len(self.graph.input))]
        self.input_names = [self.graph.input[i].name for i in range(len(self.graph.input))]
        self.output_names = [self.graph.output[i].name for i in range(len(self.graph.output))]


    @property
    def ops(self): return self.graph.node
    
    @property
    def sources(self): return [op for op in self.ops if len(op.inputs) == 0]
    
    @property
    def sinks(self): return [op for op in self.ops if len(op.outputs) == 0]
    
    @property
    def tensors(self): 
        return set(sum([list(op.outputs) for op in self.ops], []) + sum([list(op.inputs) for op in self.ops], []))
    
    def onnxToNetworkx(self, graph):
      G = nx.DiGraph()
      nlist = []
      for node in graph.node:
        # if node.op_type in self.modules_order:
        nlist.append((node.name, {'data':node, 'visited':0, 'in_insts':False}))
      # nlist = [(node.name, {'data':node, 'visited':0, 'in_insts':False}) for node in graph.node]

      elist = []
      for node in graph.node:
        for inp in node.input:
          #get nodes with output matching inp
          for n in graph.node:
            n_outputs = n.output
            for o in n_outputs:
              if o == inp:
                elist.append((n.name, node.name))
                break
      G.add_nodes_from(nlist)
      G.add_edges_from(elist)
      for node in graph.node:
        G.nodes[node.name]['visited'] = len(G.out_edges(node.name))
      return G

    def get_node_shape(self, node):
      dims = node.type.tensor_type.shape.dim
      shape = tuple([d.dim_value for d in dims])
      return shape
        
    def get_op_by_name(self, op_name):
        return self.graph.get_operation_by_name(op_name)
    
    def get_tensor_by_name(self, tensor_name):
        return self.graph.get_tensor_by_name(tensor_name)
        
    def get_upsample_info(self, upsample_op):
      """
        Extract all the info of the upsample operation
      """
      #get the channel count of the proceeding conv operation
      prev_ops = list(nx.dfs_preorder_nodes(self.reversed_network, upsample_op.name))[1:]
      channel_count = 0
      for op_name in prev_ops:
        op = self.network.nodes[op_name]['data']
        if op.op_type == 'Conv':
          channel_count = self.get_conv_info(op)['out_channels']
          break
      scales_node_name = upsample_op.input[1]
      for node in self.graph.initializer:
        # print(node.name, scales_node_name)
        if node.name == scales_node_name:
          scales = node.float_data
          break
      
      return {
          'type': upsample_op.op_type,
          'scale': scales,
          'in_channels': channel_count,
          'out_channels': channel_count
      }
    
    def get_pool_info(self, pool_op):

      #get the channel count of the proceeding conv operation
      prev_ops = list(nx.dfs_preorder_nodes(self.reversed_network, pool_op.name))[1:]

      channel_count = self.graph.input[0].type.tensor_type.shape.dim[3].dim_value
      for op_name in prev_ops:
        op = self.network.nodes[op_name]['data']
        if op.op_type == 'Conv':
          channel_count = self.get_conv_info(op)['out_channels']
          break
      
      for op_name in prev_ops:
        op = self.network.nodes[op_name]['data']
        if op.op_type == 'Concat':
          channel_count = self.get_concat_info(op)['out_channels']
          break

      if 'ENet' in self.model_name:
        for op_name in prev_ops:
          op = self.network.nodes[op_name]['data']
          if op.op_type == 'Add':
            # print(op.name)
            sucessors = list(nx.bfs_tree(self.network, op.name))[1:]
            # print(sucessors)
            for successor in sucessors:
              # print(self.network.nodes[successor]['data'].name)
              if self.network.nodes[successor]['data'].op_type == 'Conv':
                channel_count = self.get_conv_info(self.network.nodes[successor]['data'])['in_channels']
                # print(channel_count)
                break
            break
      # exit()
      return {
          'type': pool_op.op_type,
          'kernel_h': self.get_node_attribute(pool_op, 'kernel_shape').ints[0],
          'kernel_w': self.get_node_attribute(pool_op, 'kernel_shape').ints[1],
          'stride': self.get_node_attribute(pool_op, 'strides').ints[0],
          'in_channels': channel_count,
          'out_channels': channel_count
      }
        
    def get_concat_info(self, concat_op):
        """
          Return the input and output tensors of the concat operation
        """
        concat_inputs = self.network.predecessors(concat_op.name)
        out_channels = 0
        for input in concat_inputs:
          info = self.get_op_info(self.network.nodes[input]['data'])
          out_channels += info['out_channels']
          
        return {
            'type': concat_op.op_type,
            'in_channels': out_channels,
            'out_channels': out_channels
            # 'input_tensor' : in_tensors,
            # 'output_tensor' : out_tensor
        }
    
    def get_relu_info(self, op):
      prev_ops = list(nx.dfs_preorder_nodes(self.reversed_network, op.name))[1:]
      channel_count = self.graph.input[0].type.tensor_type.shape.dim[3].dim_value
      for op_name in prev_ops:
        conv_op = self.network.nodes[op_name]['data']
        if conv_op.op_type == 'Conv':
          channel_count = self.get_conv_info(conv_op)['out_channels']
          break
      return {
          'name' : op.name,
          'type': op.op_type,
          'in_channels': channel_count,
          'out_channels': channel_count
      }

    #get weights of node
    def get_node_weights(self, node):
      w_nodes = []
      for w_node in self.graph.initializer:
        if w_node.name in node.input:
          w_nodes.append(w_node)
      return w_nodes

    def get_node_weight_shapes(self, node):
      w_nodes = self.get_node_weights(node)
      w_shapes = []
      for w_node in w_nodes:
        w_shapes.append(w_node.dims)
      
      # print(w_shapes)
      if len(w_shapes)==1:
        w_shape = w_shapes[0]
        b_shape = None
      elif len(w_shapes)==2:
        if len(w_shapes[0])==1:
          w_shape = w_shapes[1]
          b_shape = w_shapes[0]
        else:
          w_shape = w_shapes[0]
          b_shape = w_shapes[1]
      else:
        print('Error: no weights found for this node')
      # print(w_shape, b_shape)
      return w_shape, b_shape

    def get_node_attribute(self, node, attr_name):
      attributes = node.attribute
      for attr in attributes:
        if attr.name == attr_name:
          return attr
      print('Error: No attribute named {} in node {}'.format(attr_name, node.name))
      return None
      

    def get_conv_info(self, conv_op):
        """
          Extract all the info of the normal/depthwise conv operation
        """

        #assuming onnx shape is [out_channels, in_channels, kernel_size, kernel_size]
        w_shape, b_shape = self.get_node_weight_shapes(conv_op)
        # print(w_shape, b_shape)
        conv_dilation = self.get_node_attribute(conv_op, 'dilations').ints[0]
        if conv_op.op_type == 'Conv':
          #group is number of input channels for depthwise conv
          conv_group = self.get_node_attribute(conv_op, 'group').i
          #nconv
          if conv_group == 1 and conv_dilation == 1:
            out_channels = w_shape[0]
            in_channels = w_shape[1] 
            kernel_h = self.get_node_attribute(conv_op, 'kernel_shape').ints[0]
            kernel_w = self.get_node_attribute(conv_op, 'kernel_shape').ints[1]
            dilation_rate = conv_dilation
            conv_type = 'NConv'
            in_tensor = conv_op.input[0]
            out_tensor = conv_op.output[0]
            stride = self.get_node_attribute(conv_op, 'strides').ints[0]
            tstride = 1
          elif conv_group == 1 and conv_dilation > 1:
            out_channels = w_shape[0]
            in_channels = w_shape[1] 
            kernel_h = self.get_node_attribute(conv_op, 'kernel_shape').ints[0]
            kernel_w = self.get_node_attribute(conv_op, 'kernel_shape').ints[1]
            dilation_rate = conv_dilation
            conv_type = 'DConv'
            in_tensor = conv_op.input[0]
            out_tensor = conv_op.output[0]
            stride = self.get_node_attribute(conv_op, 'strides').ints[0]
            tstride = 1
          elif conv_group > 1:
            out_channels = w_shape[0]
            in_channels = w_shape[1]
            kernel_h = self.get_node_attribute(conv_op, 'kernel_shape').ints[0]
            kernel_w = self.get_node_attribute(conv_op, 'kernel_shape').ints[1]
            dilation_rate = conv_dilation
            conv_type = 'DWConv'
            in_tensor = conv_op.input[0]
            out_tensor = conv_op.output[0]
            stride = self.get_node_attribute(conv_op, 'strides').ints[0]
            tstride = 1
        elif conv_op.op_type == 'ConvTranspose':
          out_channels = w_shape[1]
          in_channels = w_shape[0] 
          kernel_h = self.get_node_attribute(conv_op, 'kernel_shape').ints[0]
          kernel_w = self.get_node_attribute(conv_op, 'kernel_shape').ints[1]
          dilation_rate = conv_dilation        
          conv_type = 'TConv'
          in_tensor = conv_op.input[0]
          out_tensor = conv_op.output[0]
          stride = 1
          tstride = self.get_node_attribute(conv_op, 'strides').ints[0]
        return {
          'name': conv_op.name,
          'type': conv_type,
          # 'weight_shape': w_shape,
          'kernel_h': kernel_h,
          'kernel_w': kernel_w,
          'in_channels': in_channels,
          'out_channels': out_channels,
          # 'input_tensor' : in_tensor,
          # 'output_tensor' : out_tensor,
          'stride' : stride,
          'tstride' : tstride,
          'dilation_rate' : dilation_rate,
          'bias_en': 0 if b_shape == None else 1
          }
    
    
    @staticmethod
    def get_op_input_tensor_names(op): return [name for name in op.input]
    
    @staticmethod
    def get_op_output_tensor_names(op): return [tensor.name for tensor in op.outputs]
    
    def get_tensor_dest_ops(self, tensor, return_names=False):
        """
          Return the list of next operations
        """
        dest_ops = [op for op in self.ops if tensor.output[0] in self.get_op_input_tensor_names(op)]
        return dest_ops if not return_names else self._op_names(dest_ops)
    
    def get_op_next_ops(self, op, return_names=False):
        """
          Return the list of next operations and remove the repeated ones
        """
        # op = self._ensure_op(op)
        next_ops = sum([self.get_tensor_dest_ops(tensor) for tensor in op.outputs], [])
        # print(next_ops)
        next_ops = op
        next_ops = list(set(next_ops))
        return next_ops if not return_names else self._op_names(next_ops)
        
    def get_ops_by_filters(self, *filters, return_names=False):
        """
          Return the (filtered) operations of the graph
        """
        candidates = self.ops
        for fltr in filters:
            candidates = list(filter(fltr, candidates))
        return candidates if not return_names else self._op_names(candidates)
    
    def topo_sorted_ops(self, return_names=False, filters=None, remove_const_ops=True):
        """
          Sort the operations in the topological order
        """
        ops = self.ops
        filters = filters or []
        if remove_const_ops: filters.append(lambda op: op.op_type != 'Const')
        if filters: ops = self.get_ops_by_filters(*filters)

        op_to_idx = {op.name:i for i,op in enumerate(ops)}

        op_topo = {}
        for i, op in enumerate(ops):
          for next_op in self.get_tensor_dest_ops(op):
            op_topo[i] = op_to_idx[next_op.name]

        op_topo = [ops[i] for i in op_topo]
        return op_topo if not return_names else self._op_names(op_topo)
                
        
    @staticmethod
    def _get_op_name(name): return name.split(':')[0]
    
    @staticmethod
    def _get_input_name(name): return name
    
    @staticmethod
    def _get_tensor_name(name): return name if ':' in name else name + ':0'
    
    @staticmethod
    def _load_graph_def(orig_model_path, is_text=False):
        with tf.io.gfile.GFile(orig_model_path, 'rb') as f:
            graph_def = tf.compat.v1.GraphDef()
            if not is_text:
                graph_def.ParseFromString(f.read())
            else:
                from google.protobuf import text_format
                text_format.Parse(f.read(), graph_def) 
        return graph_def


    def _ensure_tensor(self, tensor_or_name):
        if isinstance(tensor_or_name, str):
            return self.get_tensor_by_name(tensor_or_name)
        return tensor_or_name
    
    def _ensure_op(self, op_or_name):
        if isinstance(op_or_name, str):
            return self.get_op_by_name(op_or_name)
        return op_or_name
    
    @staticmethod
    def _op_names(ops): return [op.name for op in ops]
    
    @staticmethod
    def _tensor_names(tensors): return [tensor.name for tensor in tensors]
    
    def _verify_input_shape(self, input_shape):
        """
          Return the shape of input as a list of length 4
        """
        if isinstance(input_shape[0], list):
            return [self._verify_input_shape(shape) for shape in input_shape]
        input_shape = list(input_shape)
        if len(input_shape) == 4:
            return input_shape
        elif len(input_shape) == 3:
            return [None, *input_shape]
        elif len(input_shape) == 2:
            return [None, *input_shape, 3]
        else:
            raise ValueError('Invalid input_shape:', input_shape)

    def _verify_input_name(self, name):
        """
          Check whether the input tensor exists
        """
        if isinstance(name, list):
            return [self._verify_input_name(n)[0] for n in name]
        return [self._get_op_name(name)]

    def _verify_output_name(self, name):
        """
          Check whether the output tensor exists
        """
        if isinstance(name, list):
            return [self._verify_output_name(n)[0] for n in name]
        return [self._get_tensor_name(name)]
    
    def get_insts(self):
      if 'ENet' in self.model_name:
        conv_ops = ['Conv', 'ConvTranspose']
        parallel_modules = ['MaxPool', 'Pad']
      else:
        conv_ops = ['Conv', 'ConvTranspose']#, 'MaxPool']
        parallel_modules = []
      in2_ops = ['Add', 'Mult']
      # Sort the operations in topological order
      topo_sorted = list(nx.topological_sort(self.network))
      # for node in topo_sorted:
      #   print(self.network.nodes[node]['visited'])
      insts = []
      count = 0
      for conv_node in topo_sorted:
    
        # secondary input for Add or Mult etc.
        inst = {'main_input':[], 'secondary_input':[], 'ops':[]}
        nonconv_inst = {'main_input':[], 'secondary_input':[], 'ops':[]}
        modules_list = copy.deepcopy(self.modules)
        op_visited = self.network.nodes[conv_node]['visited'] <= 0
        op_in_modules = self.network.nodes[conv_node]['data'].op_type in self.modules_order
        op_is_conv = self.network.nodes[conv_node]['data'].op_type in conv_ops
        # if self.network.nodes[conv_node]['data'].op_type=='Concat':
        #   inst['ops'].append(self.network.nodes[conv_node]['data'])
        
        if not op_visited and op_in_modules and op_is_conv:
          
          # print(self.network.nodes[conv_node]['data'].name)
          count += 1

          prev_nodes = list(nx.dfs_preorder_nodes(self.reversed_network, conv_node))[1:]
          next_nodes = list(nx.dfs_preorder_nodes(self.network, conv_node))[1:]
          
          conv_node_type = self.network.nodes[conv_node]['data'].op_type
          conv_module_idx = 0
          for i, module in enumerate(modules_list):
              if module[0] == conv_node_type and module[1] == True:
                module[1] = False
                conv_module_idx = i
                break

          self.network.nodes[conv_node]['visited'] = 0 #this might be a bug it was -= 1
          self.network.nodes[conv_node]['in_insts'] = True
          inst['ops'].append(self.network.nodes[conv_node]['data'])

          parallel_nodes = list(nx.dfs_preorder_nodes(self.reversed_network, next_nodes[0]))[1:]
          for parallel_node in parallel_nodes:
            parallel_node_type = self.network.nodes[parallel_node]['data'].op_type
            if parallel_node_type in parallel_modules and self.network.nodes[parallel_node]['visited'] >= 1:
              self.network.nodes[parallel_node]['visited'] -= 1
              self.network.nodes[parallel_node]['in_insts'] = True
              # print(self.network.nodes[parallel_node]['data'].op_type)
              inst['ops'].append(self.network.nodes[parallel_node]['data'])
              # modules_list[parallel_node_type] = False

          # if conv_node_type == 'ConvTranspose':
          #   modules_list[i][1] = False
            # for prev_node in prev_nodes:
            #   if self.network.nodes[prev_node]['visited'] >= 1:
            #     print('Error:', self.network.nodes[prev_node]['data'].name)
            # # for next_node in next_nodes:
            # #   if self.network.nodes[next_node]['visited'] >= 1:
            # #     print('Error:', self.network.nodes[next_node]['data'].name)
            # exit()
          #check predecessor nodes of current node if they can be mapped to the modules
          for prev_node in prev_nodes:
            prev_node_type = self.network.nodes[prev_node]['data'].op_type
            # print(prev_node_type)
            #go through modules proceeding the conv module
            for i in range(conv_module_idx-1,-1,-1):
              # print(self.network.nodes[prev_node]['visited'], modules_list[i][0], prev_node_type, modules_list[i][1], self.network.nodes[prev_node]['visited'] >= 1 and modules_list[i][0]==prev_node_type and modules_list[i][1]==True)
              if self.network.nodes[prev_node]['visited'] >= 1 and modules_list[i][0]==prev_node_type:
                if modules_list[i][1]==True:
                  modules_list[i][1] = False
                  self.network.nodes[prev_node]['visited'] -= 1
                  self.network.nodes[prev_node]['in_insts'] = True
                  inst['ops'].insert(0,self.network.nodes[prev_node]['data'])
                  # print('inserted', prev_node)
                  break
                else:
                  modules_list[i][1] = False
          # exit()
          for prev_node in prev_nodes:
            prev_node_type = self.network.nodes[prev_node]['data'].op_type
            if not self.network.nodes[prev_node]['in_insts'] and prev_node_type in self.modules_order:
              # modules_list[i][1] = False
              # self.network.nodes[prev_node]['visited'] = 0
              self.network.nodes[prev_node]['in_insts'] = True
              nonconv_inst['ops'].append(self.network.nodes[prev_node]['data'])
              break


          #check successor nodes of current node if they can be mapped to the modules
          for next_node in next_nodes:
            next_node_type = self.network.nodes[next_node]['data'].op_type
            
            operation_ready = True
            for inbound_node in self.network.predecessors(next_node):
              if not self.network.nodes[inbound_node]['in_insts']:
                operation_ready = False
                break

            data_needed_later = 0
            for outbound_node in self.network.successors(next_node):
              if self.network.nodes[outbound_node]['visited'] > 0:
                data_needed_later += 1
            
            #go through modules proceeding the conv module
            for i in range(conv_module_idx+1,len(modules_list)):
              if self.network.nodes[next_node]['visited'] >= 1 and modules_list[i][0]==next_node_type and modules_list[i][1]==True and operation_ready:
                modules_list[i][1] = False
                self.network.nodes[next_node]['visited'] -= 1
                self.network.nodes[next_node]['in_insts'] = True
                inst['ops'].append(self.network.nodes[next_node]['data'])
                break
              else:
                modules_list[i][1] = False
            
            if data_needed_later > 1:
              break

        if len(nonconv_inst['ops']) > 0:
          insts.append(nonconv_inst) 
        if len(inst['ops']) > 0:
          insts.append(inst)
        
      for idx, inst in enumerate(insts):
        main_op = 0
        secondary_op = 0
        inst_op_names = [op.name for op in inst['ops']]
        for op in inst['ops']:
          if op.op_type == 'Conv' or op.op_type == 'ConvTranspose' or (op.op_type == 'MaxPool' and 'UNet' in self.model_name):
            main_op = op.name
          elif op.op_type == 'Add':# or op.op_type == 'Concat':
            secondary_op = op.name
          # else:
          #   main_op = op.name
        
        if main_op != 0:
          main_op_inputs = list(nx.dfs_preorder_nodes(self.reversed_network, main_op))[1:]
          for input in main_op_inputs:
            if input not in inst_op_names:
              inst['main_input'].append(self.network.nodes[input]['data'])
              break
          if idx==0 and len(main_op_inputs)==0:
            # print(self.graph.input)
            inst['main_input'].append(self.graph.input[0])

        if secondary_op != 0:
          secondary_op_inputs = list(nx.dfs_preorder_nodes(self.reversed_network, secondary_op))[1:]
          for input in secondary_op_inputs:
            # print(secondary_op, input)
            # for name in inst_op_names:
            #   print('\t', name)
            if input not in inst_op_names:
              inst['secondary_input'].append(self.network.nodes[input]['data'])
              break
        
        for inst in insts:
          ops = inst['ops']
          last_op = ops[-1]
          inst['output_name'] = last_op.output
          
        # if idx==3:
        #   break
        # for input in secondary_op_inputs:
        #   if input not in inst_op_names:
        #     inst['secondary_input'].append(self.network.nodes[input]['data'])
        
      # for idx, inst in enumerate(insts):
      #   first_node = inst['ops'][0].name
      #   # for op in inst['ops']:
      #   #   if op.op_type == 'Conv':
      #   #     first_node = op.name
      #   inst_op_names = [op.name for op in inst['ops']]
      #   add_node = 0
      #   for op in inst['ops']:
      #     if op.op_type == 'Add' or op.op_type == 'Concat':
      #       add_node = op.name
      #       break
      #   if add_node != 0:
      #     add_inputs = self.network.predecessors(add_node)
      #     if idx == 3:
      #       print(add_input, add_inputs)
      #     for add_input in add_inputs:
      #       if add_input not in inst_op_names:
      #         inst['secondary_input'].append(self.network.nodes[add_input]['data'])

      #   first_inputs = self.network.predecessors(first_node)
      #   for inst_input in first_inputs:
      #     inst['main_input'].append(self.network.nodes[inst_input]['data'])
      # for inst in insts:
      #   first_node = 0#inst['ops'][0].name
      #   for op in inst['ops']:
      #     if op.op_type == 'Conv':
      #       first_node = op.name
      #   first_inputs = self.network.predecessors(first_node)
      #   for inst_input in first_inputs:
      #     inst['main_input'].append(self.network.nodes[inst_input]['data'])

        # second_node = 0
        # for op in inst['ops']:
        #   if op.op_type == 'MaxPool':
        #     second_node = op.name
        # second_inputs = self.network.predecessors(second_node)
        # for inst_input in second_inputs:
        #   inst['secondary_input'].append(self.network.nodes[inst_input]['data'])
      count = 0
      # for inst in insts:
      #   main_input = inst['main_input']
      #   secondary_input = inst['secondary_input']
      #   # print(main_input[0].name)
      #   # if (len(secondary_input) > 0):
      #   #   print(secondary_input[0].name)
      #   for op in inst['ops']:
      #     print(op.op_type, end=' | ')
      #     count += 1
      #   print()
      # print(count)
      return insts

    def get_enabled_modules(self, inst):
      # match operations with modules
      en = []
      modules = self.modules_order
      op_types = [op.op_type for op in inst['ops']]
      module_idx = 0
      op_idx = 0
      # print(op_types)
      while(module_idx < len(self.modules)):
        if modules[module_idx] == op_types[op_idx]:
          en.append(1)
          if op_idx+1 < len(op_types):
            op_idx += 1
          module_idx += 1
        else:
          en.append(0)
          module_idx += 1
        if sum(en)==len(op_types):
          #fill remaining en with zeros
          en += [0]*(len(modules)-len(en))
          break
      return en
    
    def get_op_info(self, op):
      info = {}
      if op.op_type == 'Conv' or op.op_type =='ConvTranspose' or op.op_type == 'MatMul':
        info = self.get_conv_info(op)
      elif op.op_type == 'MaxPool' or op.op_type == 'AveragePool':
        info = self.get_pool_info(op)
      elif op.op_type == 'Upsample':
        info = self.get_upsample_info(op)
      elif op.op_type == 'Concat':
        info = self.get_concat_info(op)
      else:
        info = self.get_relu_info(op)
      return info

    def getInstsData(self, out_file):
      insts = self.get_insts()
      conv_count = 0
      for node in self.network.nodes:
        if self.network.nodes[node]['data'].op_type == 'Conv' or self.network.nodes[node]['data'].op_type == 'ConvTranspose':
          conv_count += 1
      
      conv_1_count = 0
      conv_2_count = 0
      conv_3_count = 0
      for inst in insts:
        # for input in inst['inputs']:
        #   print(input.op_type, end=', ')
        # print('-->', end=' ')
        en = self.get_enabled_modules(inst)
        # print('inst: ', end='')
        for op in inst['ops']:
          if op.op_type == 'Conv' or op.op_type =='ConvTranspose':
            op_info = self.get_conv_info(op)
            if op_info['kernel_h']==1:
              conv_1_count += 1
            elif op_info['kernel_h']==2:
              conv_2_count += 1
            elif op_info['kernel_h']==3:
              conv_3_count += 1
        #   print(op.op_type, end=' | ')
        # print()
        # print('en  : ', end='')
        # for idx in range(len(en)):
        #   if en[idx] == 1:
        #     print(self.modules_order[idx], end=' | ')
        # print()
      # print('Total number of convs: ', conv_count)
      # print('Conv1: ', conv_1_count)
      # print('Conv2: ', conv_2_count)
      # print('Conv3: ', conv_3_count)

      all_info = []
      for inst in insts:
        
        info = {
          'en': self.get_enabled_modules(inst),
          'in_num':0,
          'out_num':0,
          'downsample_factor':0,
          'upsample_factor':0,
          'name': inst['ops'][-1].name,
          'main_input': [op.name for op in inst['main_input']],
          'secondary_input': [op.name for op in inst['secondary_input']],
          'output_name': inst['output_name'],
          'ops': []
        }
        for op in inst['ops']:
          if op.op_type == 'Concat':
            concat_info = self.get_concat_info(op)
            info['in_num'] = concat_info['in_channels']
            info['out_num'] = concat_info['out_channels']
            # info['out_num'] = 

        op_idx = 0
        for idx, module in enumerate(self.modules_order):
          op_name = module
          if info['en'][idx] == 1:
            op = inst['ops'][op_idx]
            op_info = {'name':op_name, 'params':{}}
            op_info['params'] = self.get_op_info(op)
            info['ops'].append(op_info)
            if op.op_type == 'Conv' or op.op_type == 'ConvTranspose':
              info['in_num'] = op_info['params']['in_channels']
              info['out_num'] = op_info['params']['out_channels']
              # info['downsample_factor'] = max(op_info['params']['stride'], info['downsample_factor'])
              # info['upsample_factor'] = max(op_info['params']['tstride'], info['upsample_factor'])
              op_info['params']['en'] = 1
              op_info['params']['upsample_factor'] = op_info['params']['tstride']
              op_info['params']['downsample_factor'] = op_info['params']['stride']
            elif op.op_type == 'MaxPool' or op.op_type == 'AveragePool':
              # print(op_info['params']['in_channels'])
              if 'UNet' in self.model_name:
                info['in_num'] = max(op_info['params']['in_channels'], info['in_num'])
                info['out_num'] = max(op_info['params']['out_channels'], info['out_num'])
              # info['downsample_factor'] = max(op_info['params']['stride'], info['downsample_factor'])
              # info['upsample_factor'] = max(1, info['upsample_factor'])
              op_info['params']['en'] = 1
              op_info['params']['upsample_factor'] = 1
              if 'ENet' in out_file:
                op_info['params']['downsample_factor'] = 1
              else:
                op_info['params']['downsample_factor'] = op_info['params']['stride']
              # op_info['params']['channels'] = op_info['params']['out_channels']
            elif op.op_type == 'Upsample':
              # info['in_num'] = int(op_info['params']['in_channels'])
              info['out_num'] = int(op_info['params']['out_channels'])
              # info['downsample_factor'] = max(1, info['downsample_factor'])
              # info['upsample_factor'] = max(op_info['params']['scale'][-1], info['upsample_factor'])
              op_info['params']['en'] = 1
              op_info['params']['upsample_factor'] = op_info['params']['scale'][-1]
              op_info['params']['downsample_factor'] = 1
            else:
              # print('Unknown op type: ', op.op_type)
              op_info['params']['en'] = 1
              op_info['params']['upsample_factor'] = 1
              op_info['params']['downsample_factor'] = 1
            op_idx += 1
          else:
            op_info = {'name': op_name, 'params':{}}
            op_info['params']['en'] = 0
            op_info['params']['upsample_factor'] = 1
            op_info['params']['downsample_factor'] = 1
            info['ops'].append(op_info)
        all_info.append(info)
      
      for info in all_info:
        inst_ops = info['ops']
        for op_info in inst_ops:
          info['downsample_factor'] = max(op_info['params']['downsample_factor'], info['downsample_factor'])
          info['upsample_factor'] = max(op_info['params']['upsample_factor'], info['upsample_factor'])
          # info['downsample_factor'] += op_info['params']['downsample_factor'] if op_info['params']['downsample_factor'] > 1 else 0
          # info['upsample_factor'] += op_info['params']['upsample_factor'] if op_info['params']['upsample_factor'] > 1 else 0
        if info['downsample_factor']==0:
          info['downsample_factor'] = 1
        if info['upsample_factor']==0:
          info['upsample_factor'] = 1
        info['upsample_factor'] = int(info['upsample_factor'])
        info['downsample_factor'] = int(info['downsample_factor'])
      with open(out_file+'.debug', 'w') as f:
        for inst in insts:
          main_input = inst['main_input']
          secondary_input = inst['secondary_input']
          # print(main_input[0].name)
          # if (len(secondary_input) > 0):
          #   print(secondary_input[0].name)
          for op in inst['ops']:
            print(op.op_type, end=' | ', file=f)
          print(file=f)
      with open(out_file+'.csv', 'w') as f:
        input_shape = {
          "IN_NUM": self.input_shape[2],
          "IN_H": self.input_shape[0],
          "IN_W": self.input_shape[1],
        }
        print(input_shape, file=f)
        for idx in range(0,len(all_info)):#for idx in range(3,len(all_info)): #for enet
          info = all_info[idx]
          print(info, file=f)
        # input_shape = self.graph.

    
    def run(self, out_file):
      self.getInstsData(out_file)
	  
if __name__ == "__main__":
  parser = argparse.ArgumentParser(description='Data reorganization.')
  """
    Pass the following command line arguments or change the default value
    
      -p : The location where the protobuf text file is stored (do not pass the binary format)
      -m : The name of the output file. This file will have the information needed by the hardware. You should pass the file to DSE in the next step.
      -i : The name of the json file containing format of the image
      -g : Only specify it if you have used a name for your graph in the protobuf text file. Otherwise leave it blank.
      -n : The name of the first input tensor of your graph
      -o : The name of the last tensor in your graph
  """

  parser.add_argument('-g', '--onnx', metavar='ONNX', default='./protobuf.pbtxt', help='path to protobuf text file', dest='onnx_path')
  parser.add_argument('-m', '--model', metavar='MODEL', default='network.csv', help='model description', dest='model')
  parser.add_argument('-i', '--input-config', metavar='INPUT_CONFIG', default='./input.json', help='input configuration', dest='input_config')
  parser.add_argument('-a', '--architecture', metavar='ARCHITECTURE', default='./architecture.json', help='architecture configuration', dest='arch_path')
  # parser.add_argument('-g', '--graph_name', metavar='GRAPH_NAME', default='', help='graph name', dest='graph_name')
  # parser.add_argument('-n', '--input_name', metavar='INPUT_NAME', default='image', help='input tensor name', dest='input_name')
  # parser.add_argument('-o', '--output_name', metavar='OUTPUT_NAME', default='Openpose/concat_stage7:0', help='output tensor name', dest='output_name')
  
  args = parser.parse_args()
  
  # with open(args.input_config, "r") as f:
  #   input_conf = json.loads(f.read())

  # network_in_num = input_conf["IN_NUM"]
  # network_in_h = input_conf["IN_H"]
  # network_in_w = input_conf["IN_W"]
  # input_shape = [None, network_in_h, network_in_w, network_in_num]
  # input_n = args.graph_name + args.input_name
  # output_n = args.graph_name + args.output_name

  pb_pred = PBPredictor(args.onnx_path, args.arch_path)
  pb_pred.run(args.model)
