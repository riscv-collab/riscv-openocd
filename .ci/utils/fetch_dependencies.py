#!/usr/bin/python3

import sys
import os
# well...  unfortunately we need yet another python module here
# pip install pyyaml
import yaml

input_file = sys.argv[1]

ARTIFACTORY_URL = "http://artifactory.dev.syntacore.com:8082/artifactory"
ARTIFACTORY_DIR = 'openocd_build_dependencies'

class CommandFailedError(Exception):
  pass

with open(input_file, "r") as stream:
  deps = yaml.safe_load(stream)

def run_command(cmd):
  print(cmd, file=sys.stderr, flush=True)
  if os.system(cmd) != 0:
    raise CommandFailedError("failed to succesfully execute command")

def download_file(item, desc):

  O_URL = desc['URL']
  MD5 = desc['MD5']
  HOW = desc['HOW']
  DST = desc['DST']

  if DST == "-":
    DST = item

  A_URL = f"{ARTIFACTORY_URL}/{ARTIFACTORY_DIR}/{DST}"
  O_URL = desc['URL']

  print(f"trying to fetch {DST}", file=sys.stderr, flush=True)
  for url in [A_URL, O_URL]:
    try:
      cmd = f"{HOW} -L {url} -o {DST}"
      md5_expected = f"echo {MD5} {DST} >{DST}.md5.expected"
      md5_check = f"md5sum --check {DST}.md5.expected"
      if os.path.isfile(f"{DST}"):
        print(f"{DST} exists, download SKIPPED")
      else:
        run_command(cmd)
      run_command(md5_expected)
      run_command(md5_check)
      return
    except CommandFailedError:
      print(f"could not get file from {url}")
  raise CommandFailedError(f"could not download {DST}")

for item, desc in deps.items():
  print("", file=sys.stderr, flush=True)

  download_file(item, desc)
