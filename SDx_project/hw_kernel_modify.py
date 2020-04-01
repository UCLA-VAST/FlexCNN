import argparse

def run(f_input, f_output):
  input_content = open(f_input, "r")
  output_content = open(f_output, "w")

  with open(f_input, "r") as f:
    input_content = []
    for i in f.readlines():
      input_content.append(i)

    input_content.insert(0, '#include \"pose.h\"\n')
    for i in range(len(input_content)):
#      line = input_content[i].strip('\n')
      line = input_content[i]
      if line == 'inline To Reinterpret(const From& val){\n':
        input_content[i - 1] = '//' + input_content[i - 1]
        input_content[i] = '//' + input_content[i]
        input_content[i + 1] = '//' + input_content[i + 1]
        input_content[i + 2] = '//' + input_content[i + 2]

      if line == '#include \"common_header_U1.h\"\n':
        input_content[i] = '//' + input_content[i]

      if line == 'void top_kernel(\n':
        input_content[i - 1] = 'extern "C" {\n'

      if line == '    cout << layer_id << \" \" << cur_layer_batch << endl;\n':
        input_content[i] = '//' + input_content[i]

      if i == len(input_content) - 1:
        input_content.append('}\n')
        break

  with open(f_output, "w") as f:
    for i in range(len(input_content)):
      f.writelines(input_content[i])

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description='Data reorganization.')

  parser.add_argument('-i', '--input', metavar='INPUT', required=True, help='input file', dest='input')
  parser.add_argument('-o', '--output', metavar='OUTPUT', required=True, help='output file', dest='output')

  args = parser.parse_args()
  run(args.input, args.output)
