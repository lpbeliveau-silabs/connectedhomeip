####Imports
import subprocess
import sys
import json
import os
import glob
from pathlib import Path

#Defines
BOARDS = {"BRD4161A", "BRD4186A"}
BUILDS = {"OpenThread"}
BUILD_TYPES = {("standard", ""), ("release", "\"chip_detail_logging=false chip_automation_logging=false chip_progress_logging=false is_debug=false show_qr_code=false chip_build_libshell=false enable_openthread_cli=false chip_openthread_ftd=true\"")}
building_command = './scripts/examples/gn_efr32_example.sh ./silabs_examples/{app}/efr32 ./out/custom/{app}/{network} {board} {buildArguments}'

for examples in glob.glob("./silabs_examples/*"):
    for build in BUILDS:
        for board in BOARDS:
            for build_type in BUILD_TYPES:
                parsed_path = examples.split('/')
                #Build all custom examples
                c = building_command.format(app=parsed_path[2], network=build + "/" + build_type[0], board=board, buildArguments=build_type[1])
                val = subprocess.check_call(c, shell=True)
