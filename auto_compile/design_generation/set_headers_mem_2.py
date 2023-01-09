import sys
import os
from pathlib import Path

prj_path = Path(os.getenv('PRJ_PATH'))
debugFile = open(prj_path / 'src' / 'debug.h', "w")
print("#define SYNTHESIS", file=debugFile)
print("#define U1_DataFeed0Head_MEM %d"         % (1), file=debugFile)
print("#define U1_DataFeed1Head_MEM %d"         % (1), file=debugFile)
print("#define U1_DataCollect2Head_MEM %d"      % (1), file=debugFile)
print("#define U1_DataFeed0Engine0_MEM %d"      % (1), file=debugFile)
print("#define U1_DataFeed0EngineLast_MEM %d"   % (1), file=debugFile)
print("#define U1_DataFeed1Engine0_MEM %d"      % (1), file=debugFile)
print("#define U1_DataFeed1EngineLast_MEM %d"   % (1), file=debugFile)
print("#define U1_DataCollect2Engine0_MEM %d"   % (1), file=debugFile)
print("#define U1_DataCollect2EngineLast_MEM %d"% (1), file=debugFile)
print("#define cin_load_MEM %d"                 % (0), file=debugFile)
print("#define cin_load_prev_MEM %d"            % (0), file=debugFile)
print("#define weight_load_MEM %d"              % (0), file=debugFile)
print("#define bias_load_MEM %d"                % (0), file=debugFile)
print("#define cout_write_MEM %d"               % (0), file=debugFile)

debugFile.close()

