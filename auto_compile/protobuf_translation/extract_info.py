import tensorflow as tf
import numpy as np
import argparse
import json

class PBPredictor:
    def __init__(self, pb_path, is_text=False, input_shape=None, input_name=None, output_name=None):
        super().__init__()
        #graph_name = 'TfPoseEstimator/'
        graph_name = ''
        input_shape = input_shape or [None, 384, 384, 3]
        input_name = input_name or graph_name + 'image'
        output_name = output_name or graph_name + 'Openpose/concat_stage7:0'

        self.input_shapes = self._verify_input_shape(input_shape)
        self.input_names = self._verify_input_name(input_name)
        self.output_names = self._verify_output_name(output_name)

        
        self.graph = tf.Graph()
        self.sess = tf.compat.v1.Session(graph=self.graph)
        self.graph_def = self._load_graph_def(str(pb_path), is_text=is_text)
        with self.graph.as_default():
            self.input_placeholders = [
                tf.compat.v1.placeholder(np.float32, shape=shape, name=name)
                for shape, name in zip(self.input_shapes, self.input_names)
            ]
            
            tf.import_graph_def(self.graph_def, {
                input_name: input_placeholder
                for input_name, input_placeholder in zip(self.input_names, self.input_placeholders)}, name='')
#            tf.import_graph_def(self.graph_def, name='')
        ops = []
        for op in self.graph.get_operations():
            ops.append(op.name)
            #print(op.name)
        self.output_tensors = [
            self.graph.get_tensor_by_name(output_name)
            for output_name in self.output_names
        ] 
    
    
    
    @property
    def ops(self): return self.graph.get_operations()
    
    @property
    def sources(self): return [op for op in self.ops if len(op.inputs) == 0]
    
    @property
    def sinks(self): return [op for op in self.ops if len(op.outputs) == 0]
    
    @property
    def tensors(self): 
        return set(sum([list(op.outputs) for op in self.ops], []) + sum([list(op.inputs) for op in self.ops], []))
    
        
    def get_op_by_name(self, op_name):
        return self.graph.get_operation_by_name(op_name)
    
    def get_tensor_by_name(self, tensor_name):
        return self.graph.get_tensor_by_name(tensor_name)
        
    def get_upsample_layer_info(self, upsample_op):
        upsample_op = self._ensure_op(upsample_op)
        
        input_tensors = [tensor for tensor in upsample_op.inputs if tensor.op.type != 'Const']
        in_channels = out_channels = 0
        upsample_size = 2
        in_tesnor = ''
        out_tensor = upsample_op.name
        for inp in input_tensors:
          if len(inp.shape) == 4:
            inp_shape = [d.value for d in inp.shape]
            _, _, _, out_channels = inp_shape
            in_channels = out_channels
            in_tensor = (inp.name).split(":")[0]
            break
        
        return {
            'name': upsample_op.name,
            'type': upsample_op.type,
            'kernel_size': upsample_size,
            'in_channels': in_channels,
            'out_channels': out_channels,
            'input_tensor' : in_tensor,
            'output_tensor' : out_tensor
        }
    
    def get_pool_layer_info(self, pool_op):
        pool_op = self._ensure_op(pool_op)
        return {}
        
    def get_concat_layer_info(self, concat_op):
        concat_op = self._ensure_op(concat_op)
        
        input_tensors = [tensor for tensor in concat_op.inputs if tensor.op.type != 'Const']
        in_tensors = []
        in_tensors_channels = []
        out_channels = 0
        out_tensor = concat_op.name
        for inp in input_tensors:
          in_tensors.append((inp.name).split(":")[0])
          inp_shape = [d.value for d in inp.shape]
          _, _, _, inp_channels = inp_shape
          in_tensors_channels.append(inp_channels)
          out_channels += inp_channels
          
        return {
            'name': concat_op.name,
            'type': concat_op.type,
            'in_channels': in_tensors_channels,
            'out_channels': out_channels,
            'input_tensor' : in_tensors,
            'output_tensor' : out_tensor
        }
    
    def get_conv_layer_info(self, conv_op):
        conv_op = self._ensure_op(conv_op)
              
        weight_tensors = [tensor for tensor in conv_op.inputs if tensor.op.type == 'Const']
        input_tensors = [tensor for tensor in conv_op.inputs if tensor.op.type != 'Const']
        output_tensors = [tensor for tensor in conv_op.outputs if tensor.op.type != 'Const']
        assert len(weight_tensors) == 1  # maybe bias?
        assert len(input_tensors) == 1
        assert len(output_tensors) == 1
        weight_tensor = weight_tensors[0]
        weight_shape = [d.value for d in weight_tensor.shape]
        if conv_op.type == 'Conv2D':
            kernel_size, kernel_size, in_channels, out_channels = weight_shape
        elif conv_op.type == 'DepthwiseConv2dNative':
            kernel_size, kernel_size, in_channels, _ = weight_shape
            out_channels = in_channels
        else:
            raise ValueError('Unrecognized op type: ' + conv_op.type)
            
        in_tensor = ((input_tensors[0]).name).split(":")[0]
        out_tensor = ((output_tensors[0]).name).split(":")[0]
        _, strideH, strideW, _ = conv_op.get_attr('strides')
        assert strideH == strideW
        
        return {
            'name': conv_op.name,
            'type': conv_op.type,
            'weight_shape': weight_shape,
            'kernel_size': kernel_size,
            'in_channels': in_channels,
            'out_channels': out_channels,
            'input_tensor' : in_tensor,
            'output_tensor' : out_tensor,
            'stride' : strideH,
        }
    
    
    @staticmethod
    def get_op_input_tensor_names(op): return [tensor.name for tensor in op.inputs]
    
    @staticmethod
    def get_op_output_tensor_names(op): return [tensor.name for tensor in op.outputs]
    
    def get_tensor_dest_ops(self, tensor, return_names=False):
        tensor = self._ensure_tensor(tensor)
        dest_ops = [op for op in self.ops if tensor.name in self.get_op_input_tensor_names(op)]
        return dest_ops if not return_names else self._op_names(dest_ops)
    
    def get_op_next_ops(self, op, return_names=False):
        op = self._ensure_op(op)
        next_ops = sum([self.get_tensor_dest_ops(tensor) for tensor in op.outputs], [])
        next_ops = list(set(next_ops))
        return next_ops if not return_names else self._op_names(next_ops)
        
    def get_ops_by_filters(self, *filters, return_names=False):
        candidates = self.ops
        for fltr in filters:
            candidates = list(filter(fltr, candidates))
        return candidates if not return_names else self._op_names(candidates)
    
    def topo_sorted_ops(self, return_names=False, filters=None, remove_const_ops=True):
        ops = self.ops
        filters = filters or []
        if remove_const_ops: filters.append(lambda op: op.type != 'Const')
        if filters: ops = self.get_ops_by_filters(*filters)
        op_to_idx = {op:i for i,op in enumerate(ops)}
        op_topo = {i : set(op_to_idx[next_op] for next_op in self.get_op_next_ops(op))
            for i, op in enumerate(ops)}
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
        if isinstance(name, list):
            return [self._verify_input_name(n)[0] for n in name]
        return [self._get_op_name(name)]

    def _verify_output_name(self, name):
        if isinstance(name, list):
            return [self._verify_output_name(n)[0] for n in name]
        return [self._get_tensor_name(name)]
    
    def run(self, file_name):
      topo_sorted = self.topo_sorted_ops(return_names=False)[:]

      info = {}
      all_info = {}
      layer_num = 0
      for ind, op in enumerate(topo_sorted):
        if op.type == 'Conv2D':
          new_info = self.get_conv_layer_info(op.name)
          if new_info['kernel_size'] == 1 and info['type'] == 'DepthwiseConv2dNative': # if last layer is depthwise conv
            if info['kernel_size'] != 1 and info['kernel_size'] != 3:
              print("Depthwise convoultion with kernel size of", info['kernel_size'], "is not supported in HW. You should add it to HLS design.")
            else:
              info['name'] = new_info['name']
              info['type'] = "separable_conv"
              info['out_channels'] = new_info['out_channels']
              info['output_tensor'] = new_info['output_tensor']
          else:
            if info != {}:
              layer_num += 1
              all_info[layer_num] = info
            info = new_info.copy()
            info['BatchNorm'] = 0
            info['Relu'] = 0
            info['Relu6'] = 0
            info['Add'] = 0
            info['BiasAdd'] = 0
        elif op.type == 'DepthwiseConv2dNative':
          if info != {}:
            layer_num += 1
            all_info[layer_num] = info
          info = self.get_conv_layer_info(op.name)
          info['BatchNorm'] = 0
          info['Relu'] = 0
          info['Relu6'] = 0
          info['Add'] = 0
          info['BiasAdd'] = 0
        elif op.type == 'FusedBatchNorm':
          info['BatchNorm'] = 1
          info['name'] = op.name
        elif op.type == 'Relu':
          info['Relu'] = 1
          info['name'] = op.name
        elif op.type == 'Relu6':
          info['Relu6'] = 1
          info['name'] = op.name
        elif op.type == 'Add':
          info['Add'] = 1
          info['name'] = op.name
        elif op.type == 'BiasAdd':
          info['BiasAdd'] = 1
          info['name'] = op.name
        elif op.type == 'ResizeBilinear':
          if info != {}:
            layer_num += 1
            all_info[layer_num] = info
          info = self.get_upsample_layer_info(op.name)
        elif op.type == 'Pool':
          if info != {}:
            layer_num += 1
            all_info[layer_num] = info
          info = self.get_pool_layer_info(op.name)
        elif op.type == 'ConcatV2':
          if info != {}:
            layer_num += 1
            all_info[layer_num] = info
          info = self.get_concat_layer_info(op.name)
        elif op.type == 'Shape' or op.type == 'StridedSlice' or op.type == 'Placeholder':
          continue
        else:
          print(op.type, "is not supported and is ignored. If you want to use this layer, you should add it to both HW and SW codes.")
          print("The supported layers are: Conv2D, DepthwiseConv2dNative, FusedBatchNorm, Relu, Relu6, Add, BiasAdd, ResizeBilinear, Pool, ConcatV2")
      
      if info != {}:
        layer_num += 1
        all_info[layer_num] = info
        
      model = open(file_name, "w")
      model.writelines("[Name,InputTensor],Type,Inchannel,Outchannel,ExpansionFactor,Filter,Stride,Relu,Relu6,Batchnorm,BiasAdd,Add\n")
      keys = ['name', 'type', 'in_channels', 'out_channels', 'kernel_size', 'ExpansionFactor', 'stride', 'Relu', 'Relu6', 'Batchnorm', 'BiasAdd', 'Add']
      for i in all_info:
        info = all_info[i]
        if info['type'] == 'ResizeBilinear':
          info_layer = [[info['name'],info['input_tensor']], 'upsample', info['in_channels'], info['out_channels'], info['kernel_size'], 1, 1, 0, 0, 0, 0, 0]
        elif info['type'] == 'Pool':
          info_layer = []
        elif info['type'] == 'ConcatV2':
          info_layer = [info['name'], info['type'], info['in_channels'], info['out_channels']]
          for inf in info['input_tensor']:
            info_layer.append(inf)
        else:
          info_layer = [[info['name'],info['input_tensor']], info['type'], info['in_channels'], info['out_channels'], 1, info['kernel_size'], info['stride'], info['Relu'], info['Relu6'], info['BatchNorm'], info['BiasAdd'], info['Add']]
        model.writelines(";".join(str(inf) for inf in info_layer) + "\n")
		
      model.close()
	  
if __name__ == "__main__":
  parser = argparse.ArgumentParser(description='Data reorganization.')

  parser.add_argument('-p', '--pbtxt', metavar='PBTXT', default='./protobuf.pbtxt', help='path to protobuf text file', dest='pbtxt_path')
  parser.add_argument('-m', '--model', metavar='MODEL', default='./network.model', help='model description', dest='model')
  parser.add_argument('-i', '--input-config', metavar='INPUT_CONFIG', default='./input.json', help='input configuration', dest='input_config')
  parser.add_argument('-g', '--graph_name', metavar='GRAPH_NAME', default='', help='graph name', dest='graph_name')
  parser.add_argument('-n', '--input_name', metavar='INPUT_NAME', default='image', help='input tensor name', dest='input_name')
  parser.add_argument('-o', '--output_name', metavar='OUTPUT_NAME', default='Openpose/concat_stage7:0', help='output tensor name', dest='output_name')
  
  args = parser.parse_args()
  
  with open(args.input_config, "r") as f:
    input_conf = json.loads(f.read())

  network_in_num = input_conf["IN_NUM"]
  network_in_h = input_conf["IN_H"]
  network_in_w = input_conf["IN_W"]
  input_shape = [None, network_in_h, network_in_w, network_in_num]
  input_n = args.graph_name + args.input_name
  output_n = args.graph_name + args.output_name
  
  pb_pred = PBPredictor(args.pbtxt_path, is_text=True, input_name=input_n, output_name=output_n, input_shape=input_shape)
  pb_pred.run(args.model)
