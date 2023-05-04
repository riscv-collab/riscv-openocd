#!/usr/bin/env python3

import sys
import os

ARTIFACTORY_URL = "http://artifactory.dev.syntacore.com:8082/artifactory"
ARTIFACTORY_DIR = 'openocd_build_dependencies'

# NOTE: originally this file was an external yaml. However recent updates
# to docker image render pyyaml library as unusable without venv environment.
# To reduce the scope of changes it was decided to just embed dependencies into
# python script
deps = {
  "dejagnu-1.6.3.tar.gz" : {
    "URL": "https://ftp.gnu.org/gnu/dejagnu/dejagnu-1.6.3.tar.gz",
    "MD5": "68c5208c58236eba447d7d6d1326b821",
    "HOW": "curl",
    "DST": "-"
  },

  "libusb-1.0.26.tar.bz2" : {
    "URL": "https://github.com/libusb/libusb/releases/download/v1.0.26/libusb-1.0.26.tar.bz2",
    "MD5": "9c75660dfe1d659387c37b28c91e3160",
    "HOW": "curl",
    "DST": "-"
  },

  "libftdi1-1.5.tar.bz2" : {
    "URL": "https://www.intra2net.com/en/developer/libftdi/download/libftdi1-1.5.tar.bz2",
    "MD5": "f515d7d69170a9afc8b273e8f1466a80",
    "HOW": "curl",
    "DST": "-"
  },

  "hidapi-0.12.0.tar.gz" : {
    "URL": "https://github.com/libusb/hidapi/archive/refs/tags/hidapi-0.12.0.tar.gz",
    "MD5": "d0e344f2c75aba08908ce26d3fb17d74",
    "HOW": "curl",
    "DST": "-"
  }
}
class CommandFailedError(Exception):
  pass

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
