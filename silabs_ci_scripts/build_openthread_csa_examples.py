####Imports
import subprocess
import sys
import json
from pathlib import Path

#Defines
APPS = {"lighting-app", "lock-app", "light-switch-app"}
BOARDS = {"BRD4161A", "BRD4186A"}
BUILDS = {"OpenThread"}
BUILD_TYPES = {("standard", ""), ("release", "\"chip_detail_logging=false chip_automation_logging=false chip_progress_logging=false is_debug=false show_qr_code=false chip_build_libshell=false enable_openthread_cli=false chip_openthread_ftd=true\"")}
building_command = './scripts/examples/gn_efr32_example.sh ./examples/{app}/efr32 ./out/CSA/{app}{network} {board} {buildArguments}'

#Build everything
for build in BUILDS:
    for app_name in APPS:
        for board in BOARDS:
            for build_type in BUILD_TYPES:

                #Build all examples
                c = building_command.format(app=app_name, network= "/" + build + "/" + build_type[0], board=board, buildArguments=build_type[1])
                val = subprocess.check_call(c, shell= True)
