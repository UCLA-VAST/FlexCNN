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

from networkx.drawing.nx_pydot import write_dot
from collections import Counter
import itertools
from tqdm import tqdm

def contains(small, big):
  for i in range(len(big)-len(small)+1):
    for j in range(len(small)):
      if big[i+j] != small[j]:
        break
    else:
      return True
  return False

class Mapper:
  def __init__(self, onnx_path):
    super().__init__()

    self.onnx_graph = onnx.load(onnx_path).graph
    self.model = self.onnxToNetworkx(self.onnx_graph, onnx_path)

    # get modules from the onnx graph
    dram_modules, single_input_modules, double_input_modules = self.getModelModules()
    modules = dram_modules + single_input_modules + double_input_modules
    longest_path = len(single_input_modules) + len(double_input_modules)
    print('longest path:', longest_path)
    all_sequences = []
    for node_name in self.model.nodes():
      descendants, descendant_types = self.getDescendants(self.model, node_name, longest_path)
      for descendant_type in descendant_types:
        descendant_list = []
        # add elements from descendant_type to descendant_list until a repeat is found
        for i in range(len(descendant_type)):
          if descendant_type[i] not in descendant_list:
            descendant_list.append(descendant_type[i])
          else:
            break
        all_sequences.append(str(descendant_list))

    all_sequences = Counter(all_sequences)
    path_modules = []
    common_path_modules = []
    path_modules.append(str([]))
    common_path_modules.append(str([]))
    for sequence in all_sequences:
      sequence = eval(sequence)
      
      split_sequences = []
      if 'DRAM' in sequence:
        sequence.remove('DRAM')
      print(sequence)
      double_input_module_list = [module[0] for module in double_input_modules]
      # split sequence according to double input modules
      for i, module in enumerate(sequence):
        if module in double_input_module_list:
          module_idx = sequence.index(module)
          split_sequences.append(sequence[:module_idx])
          sequence = sequence[module_idx+1:]
      split_sequences.append(sequence)
      if len(split_sequences) == 1:
        path_modules.append(str(split_sequences[0]))
      elif len(split_sequences) == 2:
        path_modules.append(str(split_sequences[0]))
        common_path_modules.append(str(split_sequences[1]))
      elif len(split_sequences) == 3:
        path_modules.append(str(split_sequences[0]))
        path_modules.append(str(split_sequences[1]))
        common_path_modules.append(str(split_sequences[1]))
        common_path_modules.append(str(split_sequences[2]))

    print()
    # new_paths = []
    # for path1 in path_modules:
    #   path1 = eval(path1)
    #   for path2 in path_modules:
    #     path2 = eval(path2)
    #     new_path = []
    #     if len(path1)==len(path2):
    #       for i in range(min(len(path1), len(path2))):
    #         if path1[i] == path2[i]:
    #           new_path.append(path1[i])
    #         else:
    #           new_path.append(path1[i])
    #           new_path.append(path2[i])
    #     if len(new_path)>0 and len(set(new_path)) == len(new_path):
    #       new_paths.append(str(new_path))
    # new_paths = list(set(new_paths))
    # print(len(new_paths))
    # for path in new_paths:
    #   print(path)
    # exit()

    # path_modules += new_paths
    # common_path_modules += new_paths
    # new_paths = []
    if 'ENet' in onnx_path:
      actBNs = ['BatchNormalization', 'PReLU', 'Relu']
      # get permutations of actBN
      actBN_permutations = [str(list(perm)) for perm in list(itertools.permutations(actBNs))]
      common_path_modules += actBN_permutations

    path_modules = set(path_modules)
    common_path_modules = set(common_path_modules)

    print(30*'-' +'path modules'+30*'-')
    for module in path_modules:
      print(module)
    print(30*'-' +'common path modules'+30*'-')
    for module in common_path_modules:
      print(module)
    print()

    all_modules = [module[0] for module in single_input_modules + double_input_modules]
    path_1 = []
    path_2 = []
    common = []

    for module1 in path_modules:
      module1 = eval(module1)
      for module2 in path_modules:
        module2 = eval(module2)
        double_input_module_list = [module[0] for module in double_input_modules]
        di_module_permutations = list(itertools.permutations(double_input_module_list))
        for di_module_permutation in di_module_permutations:
          di_module_permutation = list(di_module_permutation)
          for module3 in common_path_modules:
            module3 = eval(module3)
            module3 = di_module_permutation + module3
            modules_tmp = module1 + module2 + module3
            # if modules does not have repeated items
            if len(modules_tmp) == len(all_modules) and len(set(modules_tmp)) == len(modules_tmp):
              if len(module1)==0:
                path_1.append([])
              else:
                path_1.append(module1.copy())
              if len(module2)==0:
                path_2.append([])
              else:
                path_2.append(module2.copy())
              if len(module3)==0:
                common.append([])
              else:
                common.append(module3.copy())
    # path_1.append(['Conv', 'Clip', 'Pad', 'GlobalAveragePool'])
    # path_2.append([])
    # common.append([])
    for l1, l2, l3 in tqdm(zip(path_1, path_2, common)):
      print(l1, l2, l3)
    
    results = []
    idx = 1
    print('generated %d design points'%len(path_1))
    for l1, l2, l3 in tqdm(zip(path_1, path_2, common)):
      l1.insert(0, dram_modules[0][0])
      l2.insert(0, dram_modules[1][0])
      l3.append(dram_modules[2][0])
      
      for node_name in self.model.nodes:
        self.model.nodes[node_name]['visited'] = 1
      architecture = [l1, l2, l3]
      self.arch = self.makeArchGraph(modules, *architecture)
      write_dot(self.arch, 'arch_%d.dot'%idx)
      bundles = self.algorithm(architecture)
      results.append((architecture, bundles, len(bundles)))
      idx += 1
    results.sort(key=lambda x: x[2])
    print('found %d designs'%len(results))
    for result in results:
      architecture, bundles, bundles_num = result
      bundles_types = self.bundlesToBundleTypes(bundles)
      path1_list = architecture[0]
      
      path2_list = architecture[1]
      common_path_list = architecture[2]
      print('-'*20 + ' ' + str(bundles_num) + ' ' + '-'*20)
      for path in path1_list:
        print(path)
      print()
      for path in path2_list:
        print(path)
      print()
      for path in common_path_list:
        print(path)
      print()
      for bundle in bundles_types:
        print(bundle)
      print(50*'=')
    # store results as json file
    with open('results.json', 'w') as f:
      json.dump(results, f)

  def bundlesToBundleTypes(self, bundles):
    bundle_types = []
    for bundle in bundles:
      new_bundle = []
      for node_name in bundle[1]:
        node = self.model.nodes[node_name]
        node_type = node['type']
        new_bundle.append(node_type)
      bundle_types.append((bundle[0], new_bundle))
    return bundle_types

  def getModelModules(self):
    dram_modules = [
      ('DRAM_in_1'    , {'type':'DRAM', 'used':False}),
      ('DRAM_in_2'    , {'type':'DRAM', 'used':False}),
      ('DRAM_out', {'type':'DRAM', 'used':False})
    ]
    single_input_types = []
    double_input_types = []
    for node_name in self.model.nodes:
      node = self.model.nodes[node_name]
      node_type = node['type']
      if node_type != 'DRAM':
        if self.model.in_degree(node_name) == 1:
          single_input_types.append(node_type)
        elif self.model.in_degree(node_name) == 2:
          double_input_types.append(node_type)

    single_input_types = set(single_input_types)
    double_input_types = set(double_input_types)

    single_input_modules = []
    double_input_modules = []
    for node_type in single_input_types:
      single_input_modules.append((node_type, {'type':node_type, 'used':False}))
    for node_type in double_input_types:
      double_input_modules.append((node_type, {'type':node_type, 'used':False}))

    return dram_modules, single_input_modules, double_input_modules

  def getDescendants(self, model, node_name, max_path_length):
    descendants_list = []
    descendants_types_list = []
    descendants = list(nx.dfs_preorder_nodes(model, node_name))
    successors = list(model.successors(node_name))
    for successor in successors:
      idx = descendants.index(successor)
      path = [node_name] + descendants[idx:idx+max_path_length]
      path_types = [model.nodes[node]['type'] for node in path]
      descendants_list.append(path)
      descendants_types_list.append(path_types)
    return descendants_list, descendants_types_list
  
  def getPaths(self, path_id):
    paths = [[node for node in path] for path in nx.all_simple_paths(self.arch, source='DRAM_in_%d'%path_id, target='DRAM_out')]
    paths_types = [[self.arch.nodes[node]['type'] for node in path] for path in paths]
    print('-'*50)
    for path_type in paths_types:
      print(path_type)
    print('-'*50)  
    for path in paths:
      path.remove('DRAM_in_%d'%path_id)
      path.remove('DRAM_out')
    for path in paths_types:
      path.remove('DRAM')
      path.remove('DRAM')
    return paths, paths_types

  def comparePaths(self, path1, path2):
    flag = True
    if len(path1)==0:
      return False
    for i in range(min(len(path1), len(path2))):
      if path1[i] != path2[i]:
        flag = False
        break
    return flag

  def algorithm(self, architecture):
    sorted_graph = list(nx.topological_sort(self.model))
    bundles = []
    for node_name in sorted_graph:
      layer = self.model.nodes[node_name]
      descendants, descendant_types = self.getDescendants(self.model, node_name, 5)

      arch_graph = self.arch.copy()
      
      if layer['visited']>0 and not layer['type']=='DRAM':
        counter = 0
        for descendant_path, descendant_path_types in zip(descendants, descendant_types):
          bundle = []
          
          for path_id in range(1,3):
            path_idx = path_id
            paths, paths_types = self.getPaths(path_id)
            path_found = False
            for path, path_type in zip(paths, paths_types):
              
              good_path = self.comparePaths(path_type, descendant_path_types)
              path_found = good_path
              
              if good_path:
                for i in range(len(path)):
                  if self.isReady(descendant_path[i]):
                    if self.isNeededLater(descendant_path[i]):
                      bundle.append(descendant_path[i])
                      self.model.nodes[descendant_path[i]]['visited'] -= 1
                      path_idx = path_id
                      # counter += 1
                      break
                    else:
                      bundle.append(descendant_path[i])
                      self.model.nodes[descendant_path[i]]['visited'] -= 1
                      path_idx = path_id
                if self.model.out_degree(node_name)==2 and bundle==[node_name]:
                  counter += 1
                break #break if path is found
              
            if path_found and counter < 2:
              bundles.append((path_idx, bundle))
              break
            

      elif layer['visited']>0 and layer['type']=='DRAM':
        layer['visited'] = 0

    new_bundles = []
    path_1 = architecture[0]
    path_2 = architecture[1]
    common = architecture[2]

    i = 0
    while i < len(bundles)-1:
      curr_bundle_id = bundles[i][0]
      next_bundle_id = bundles[i+1][0]
      curr_bundle = bundles[i][1]
      next_bundle = bundles[i+1][1]
      isInPath1 = False
      isInPath2 = False
      if curr_bundle_id != next_bundle_id:
        if(curr_bundle_id == 1 and next_bundle_id == 2):
          
          for node_name in curr_bundle:
            node_type = self.model.nodes[node_name]['type']
            if node_type in path_1: 
              isInPath1 = True
              break

          for node_name in next_bundle:
            node_type = self.model.nodes[node_name]['type']
            if node_type in path_2: 
              isInPath2 = True
              break

        elif (curr_bundle_id == 2 and next_bundle_id == 1):
          
          for node_name in curr_bundle:
            node_type = self.model.nodes[node_name]['type']
            if node_type in path_2: 
              isInPath2 = True
              break

          for node_name in next_bundle:
            node_type = self.model.nodes[node_name]['type']
            if node_type in path_1: 
              isInPath1 = True
              break

        else:
          print('Error')

      if isInPath1 and isInPath2:
        new_bundle = curr_bundle + next_bundle
        new_bundle_id = (curr_bundle_id, next_bundle_id)
        new_bundles.append((new_bundle_id, new_bundle))
        i += 1
      else:
        new_bundles.append(bundles[i])
      i += 1
    new_bundles.append(bundles[i])   
    return new_bundles

  def isReady(self, node):
    predecessors = self.model.predecessors(node)
    ready = True
    for predecessor in predecessors:
      if self.model.nodes[predecessor]['visited']>0:
        ready = False
        break
    return ready

  def isNeededLater(self, node):
    successors = self.model.successors(node)
    needed = False
    for successor in successors:
      predecessors = self.model.predecessors(successor)
      for predecessor in predecessors:
        if predecessor != node and not self.isReady(predecessor):
          needed = True
          break
    return needed

  def onnxToNetworkx(self, graph, onnx_path):
    unsupported_layers = ['GlobalAveragePool', 'MatMul', 'Softmax', 'Squeeze']
    G = nx.DiGraph()
    nlist = []
    for node in graph.node:
      if node.op_type in unsupported_layers:
        print('Unsupported layer: %s'%node.op_type)
        continue
      elif node.op_type == 'Transpose':
        nlist.append((node.name, {'data':node, 'type':'DRAM', 'visited':1}))
      elif node.op_type == 'ConvTranspose':
        nlist.append((node.name, {'data':node, 'type':'Conv', 'visited':1}))
      elif node.op_type == 'Concat':
        nlist.append((node.name, {'data':node, 'type':node.op_type, 'visited':1}))
      else:
        nlist.append((node.name, {'data':node, 'type':node.op_type, 'visited':1}))
    
    elist = []
    for node in graph.node:
      for inp in node.input:
        #get nodes with output matching inp
        for n in graph.node:
          n_outputs = n.output
          for o in n_outputs:
            if o == inp and node.op_type not in unsupported_layers and n.op_type not in unsupported_layers:
              elist.append((n.name, node.name))
              break
    G.add_nodes_from(nlist)
    G.add_edges_from(elist)
    # for node in G.nodes:
    #   if G.out_degree(node) == 2:
    #     G.nodes[node]['visited'] = 2
    # remove softmax node
    # for node in G.nodes:
    #   if G.nodes[node]['type'] == 'Softmax':
    #     G.remove_node(node)
    #   elif G.nodes[node]['type'] == 'GlobalAveragePool':
    #     G.remove_node(node)
    #   elif G.nodes[node]['type'] == 'Squeeze':
    #     G.remove_node(node)
    #   elif G.nodes[node]['type'] == 'MatMul':
    #     G.remove_node(node)
    
    prelu_count = 0
    if 'ENet' in onnx_path:
      for node in G.nodes:
        if 'Transpose' in node and node in G.nodes:
          for successor in G.successors(node):
            G = nx.contracted_nodes(G, node, successor, self_loops=False)
          for successor in G.successors(node):
            # print(node, successor)
            G = nx.contracted_nodes(G, node, successor, self_loops=False)
          for successor in G.successors(node):
            # print(node, successor)
            G = nx.contracted_nodes(G, node, successor, self_loops=False)
          for successor in G.successors(node):
            # print(node, successor)
            G = nx.contracted_nodes(G, node, successor, self_loops=False)
          for successor in G.successors(node):
            output_node =  G.nodes[successor]['data'].output[0]
            # print(output_node)
            G = nx.contracted_nodes(G, node, successor, self_loops=False)
          for predecessor in G.predecessors(node):
            pred_data = G.nodes[predecessor]['data']
            if pred_data.op_type == 'Relu':
              # print(node, predecessor)
              G = nx.contracted_nodes(G, node, predecessor, self_loops=False)
          prelu_count += 1
          mapping = {node: 'PReLU_'+str(prelu_count)}
          G = nx.relabel_nodes(G, mapping)
          node_data = G.nodes[mapping[node]]['data']
          node_data.name = mapping[node]
          node_data.op_type = 'PReLU'
          node_data.output[0] = output_node
          G.nodes[mapping[node]]['type'] = 'PReLU'
    
    return G
  
  def makeArchGraph(self, modules, path1, path2, common_path):
    G = nx.DiGraph()
    nlist = modules
    elist = []
    
    for i in range(len(path1)):
      for j in range(i+1, len(path1)):
        elist.append((path1[i], path1[j]))
      for j in range(0, len(common_path)):
        elist.append((path1[i], common_path[j]))
    # for edge in elist:
    #   print(edge)
    # print()
    for i in range(len(path2)):
      for j in range(i+1, len(path2)):
        elist.append((path2[i], path2[j]))
      for j in range(0, len(common_path)):
        elist.append((path2[i], common_path[j]))
    # for edge in elist:
    #   print(edge)
    # print()
    for i in range(len(common_path)):
      for j in range(i+1, len(common_path)):
        elist.append((common_path[i], common_path[j]))
    # for edge in elist:
    #   print(edge)
    G.add_nodes_from(nlist)
    G.add_edges_from(elist)
    return G
    
if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Data reorganization.')

  parser.add_argument('-g', '--onnx', metavar='ONNX', default='./protobuf.pbtxt', help='path to protobuf text file', dest='onnx_path')

  args = parser.parse_args()

  mapper = Mapper(args.onnx_path)
  # mapper.run(args.model)
