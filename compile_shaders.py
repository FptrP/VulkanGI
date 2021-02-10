#!/usr/bin/python3
import sys
from os import listdir, path, system
from os.path import isfile, join

SHADER_EXT = [".frag", ".vert"]
SPV_EXT = ".spv"
DIR = "./src/shaders/"

def compile_all():
  files = [f for f in listdir(DIR) if isfile(join(DIR, f))]
  files_ext = map(lambda f: path.splitext(f), files)
  for (name, ext) in files_ext:
    if not ext in SHADER_EXT: 
      continue
    out_name = name +  "_" + ext[1:] + SPV_EXT
    out_name = join(DIR, out_name)
    print("Compile {}{} -o {}".format(join(DIR, name), ext, out_name))
    system("glslc {}{} -o {}".format(join(DIR, name), ext, out_name))

def clean():
  files = [f for f in listdir(DIR) if isfile(join(DIR, f))]
  files_ext = map(lambda f: path.splitext(f), files)
  for (name, ext) in files_ext:
    if ext == SPV_EXT:
      cmd = "rm {}{}".format(join(DIR, name), ext)
      print(cmd) 
      system(cmd)


if "clean" in sys.argv:
  clean()
else:
  compile_all()