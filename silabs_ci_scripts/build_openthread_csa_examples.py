####Imports
import subprocess
import sys
import json
from pathlib import Path


#Defines
BUILDS = {"OpenThread"}
standard_build = ("standard", "")
release_build = ("release",
                                      "\"chip_detail_logging=false chip_automation_logging=false chip_progress_logging=false is_debug=false show_qr_code=false chip_build_libshell=false enable_openthread_cli=false chip_openthread_ftd=true\"")
building_command = './scripts/examples/gn_efr32_example.sh ./examples/{app}/efr32 ./out/CSA/{app}{network} {board} {buildArguments}'


if (len(sys.argv) > 1):
    APPS = {sys.argv[1]}
else:
    APPS = {"lighting-app", "lock-app", "light-switch-app", "window-app"}

if (len(sys.argv) > 2):
    BOARDS = {sys.argv[2]}
else:
    BOARDS = {"BRD4161A", "BRD4186A"}

if (len(sys.argv) > 3):
    if(sys.argv[3] == "standard"):
        BUILD_TYPES = {standard_build}
    elif(sys.argv[3] == "release"):
        BUILD_TYPES = {release_build}
else:
    BUILD_TYPES = {standard_build, release_build}


print(BUILD_TYPES)
print(BOARDS)
print(APPS)

#Build everything
for app_name in APPS:
    for build in BUILDS:
        for board in BOARDS:
            for build_type in BUILD_TYPES:

                #Build all examples
                c = building_command.format(app=app_name, network= "/" + build + "/" + build_type[0], board=board, buildArguments=build_type[1])
                val = subprocess.check_call(c, shell= True)

