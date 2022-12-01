#!/usr/bin/python3

import sys
import os
# well...  unfortunately we need yet another python module here
# pip install pyyaml
import yaml

input_file = sys.argv[1]

ARTIFACTORY_URL = "http://artifactory.dev.syntacore.com:8082/artifactory"
ARTIFACTORY_DIR = 'openocd_build_dependencies'
os.putenv('ARTIFACTORY_DIR', ARTIFACTORY_DIR)
os.putenv('SKIP_ARCHIVATION', 'true')

class CommandFailedError(Exception):
  pass

with open(input_file, "r") as stream:
  deps = yaml.safe_load(stream)

def run_command(cmd):
  print(cmd)
  if os.system(cmd) != 0:
    raise CommandFailedError("failed to succesfully execute command")

for item, desc in deps.items():
  URL = desc['URL']
  MD5 = desc['MD5']
  HOW = desc['HOW']
  DST = desc['DST']
  if DST == "-":
    DST = item

  print("")
  if os.getenv('DOWNLOAD_DEPENDENCIES_FROM_ARTIFACTORY', 'false') == 'true':
    URL = f"{ARTIFACTORY_URL}/{ARTIFACTORY_DIR}/{DST}"
  cmd = f"{HOW} -L {URL} -o {DST}"
  md5_expected = f"echo {MD5} {DST} >{DST}.md5.expected"
  md5_check = f"md5sum --check {DST}.md5.expected"
  if os.path.isfile(f"{DST}"):
    print(f"{DST} exists, download SKIPPED")
  else:
    run_command(cmd)
  run_command(md5_expected)
  run_command(md5_check)

  if os.getenv('ARTIFACTORY_UPLOAD', 'false') == 'true':
    utils_dir = os.path.dirname(__file__)
    upload_command = f"{utils_dir}/upload_artifact.sh {DST}"
    run_command(upload_command)
