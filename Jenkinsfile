#!groovy
@Library('gsdk-shared-lib@master')
import groovy.json.JsonSlurper

buildOverlayDir = ''

// helpers
def initWorkspaceAndScm()
{
    buildOverlayDir = sh( script: '/srv/jenkins/createSuperOverlay.sh '+
                                  'createbuildoverlay '+
                                  '/srv/workspaces '+
                                  '/srv/jenkins/reference',
                                  returnStdout: true ).trim()
    echo "Build overlay directory: ${buildOverlayDir}"

    // Get pipeline metadata
    dir(buildOverlayDir+createWorkspaceOverlay.overlayMatterPath)
    {
        checkout scm: [$class                     : 'GitSCM',
                 branches                         : scm.branches,
                 browser: [$class: 'Stash',
                            repoUrl: 'https://stash.silabs.com/projects/WMN_TOOLS/repos/matter/'],
                 doGenerateSubmoduleConfigurations: scm.doGenerateSubmoduleConfigurations,
                 extensions                       : scm.extensions << [$class: 'ScmName', name: 'matter'],
                 userRemoteConfigs                : scm.userRemoteConfigs]

        // Matter Init --Checkout relevant submodule
        sh 'scripts/checkout_submodules.py --shallow --platform efr32'
    }

}

buildFarmLabel = 'Build-Farm'

def runInWorkspace(Map args, Closure cl)
{
    return {
        actionWithRetry {
            label = args.label ?: buildFarmLabel
            node(label) {
                def workspaceTmpDir = createWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                                             buildOverlayDir)
                def filterPattern = args.saveFilter == 'NONE' ? '-name "no-files"' : args.saveFilter

                def dirPath = workspaceTmpDir + (args.wsPath ?: createWorkspaceOverlay.overlayMatterPath)
                def saveDir = args.savePath ?: 'matter/'
                dir(dirPath) {
                    // pass environment in as arg if possible...
                    def env = ['BASIC_ENV=1']
                    withEnv(env) {
                        sh 'printenv | sort'
                        try {
                            cl()
                        } catch (e) {
                            deactivateWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                                       workspaceTmpDir,
                                                       saveDir,
                                                       '-name no-files')
                            throw e
                        }
                    }
                }
                if (filterPattern != null) {
                    // use a custom filter (including an empty '')
                    deactivateWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                             workspaceTmpDir,
                                             saveDir, filterPattern)
                } else {
                    deactivateWorkspaceOverlay(advanceStageMarker.getBuildStagesList(), workspaceTmpDir)
                }
            }
        }
    }
}

def bootstrapWorkspace()
{
    actionWithRetry {
        node(buildFarmLabel)
        {
            def workspaceTmpDir = createWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                                            buildOverlayDir)
            def dirPath = workspaceTmpDir + createWorkspaceOverlay.overlayMatterPath
            def saveDir = 'matter/'
            dir(dirPath) {

                withDockerContainer(image: "connectedhomeip/chip-build-efr32:0.5.64", args: "-u root ")
                {
                    sh './scripts/build/gn_bootstrap.sh'
                }
            }
            deactivateWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                       workspaceTmpDir,
                                       'matter/',
                                       '-name "*" ! -name "*.lock"')
        }
    }
}

def buildOpenThreadExample(String name, String board)
{
    actionWithRetry {
        node(buildFarmLabel)
        {
            def workspaceTmpDir = createWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                                            buildOverlayDir)
            def dirPath = workspaceTmpDir + createWorkspaceOverlay.overlayMatterPath
            def saveDir = 'matter/'
            dir(dirPath) {
                withDockerContainer(image: "connectedhomeip/chip-build-efr32:0.5.64", args: "-u root")
                {
                    // CSA Examples build
                    withEnv(['PW_ENVIRONMENT_ROOT='+dirPath])
                    {
                      	sh "python3 ./silabs_ci_scripts/build_openthread_csa_examples.py \"${name}\" \"${board}\""
                    }
                }
            }
            deactivateWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                       workspaceTmpDir,
                                       'matter/out/',
                                       '-name "*.s37" -o -name "*.map"')
        }
    }
}



def buildSilabsCustomOpenThreadExamples(String board)
{
    actionWithRetry {
        node(buildFarmLabel)
        {
            def workspaceTmpDir = createWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                                            buildOverlayDir)
            def dirPath = workspaceTmpDir + createWorkspaceOverlay.overlayMatterPath
            def saveDir = 'matter/'
            dir(dirPath) {
                withDockerContainer(image: "connectedhomeip/chip-build-efr32:0.5.64", args: "-u root")
                {
                    // Custom Silabs build
                    withEnv(['PW_ENVIRONMENT_ROOT='+dirPath])
                    {
                        sh "echo ${board}"
                        sh "echo $board"
                        sh "echo \"${board}\""
                        sh "python3 ./silabs_ci_scripts/build_custom_examples.py \"${board}\""
                    }
                }
            }
            deactivateWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                       workspaceTmpDir,
                                       'matter/out/',
                                       '-name "*.s37" -o -name "*.map"')
        }
    }
}

def buildWiFiLighting()
{
    actionWithRetry {
        node(buildFarmLabel)
        {
            def workspaceTmpDir = createWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                                            buildOverlayDir)
            def dirPath = workspaceTmpDir + createWorkspaceOverlay.overlayMatterPath
            def saveDir = 'matter/'
            dir(dirPath) {
                withDockerContainer(image: "connectedhomeip/chip-build-efr32:0.5.64", args: "-u root")
                {
                  	withEnv(['PW_ENVIRONMENT_ROOT='+dirPath])
                    {
                      	sh './scripts/examples/gn_efr32_example.sh examples/lighting-app/efr32/ out/light_app_wifi_wf200 BRD4161A  "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi wf200'
                        sh './scripts/examples/gn_efr32_example.sh examples/lighting-app/efr32/ out/light_app_wifi_rs9116 BRD4161A  "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi rs911x'
                    }
                }
            }
            deactivateWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                       workspaceTmpDir,
                                       'matter/',
                                       '-name "*.s37" -o -name "*.map"')
        }
    }
}

def buildWiFiLock()
{
    actionWithRetry {
        node(buildFarmLabel)
        {
            def workspaceTmpDir = createWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                                            buildOverlayDir)
            def dirPath = workspaceTmpDir + createWorkspaceOverlay.overlayMatterPath
            def saveDir = 'matter/'
            dir(dirPath) {
                withDockerContainer(image: "connectedhomeip/chip-build-efr32:0.5.64", args: "-u root")
                {
                  	withEnv(['PW_ENVIRONMENT_ROOT='+dirPath])
                    {
                      	sh './scripts/examples/gn_efr32_example.sh examples/lock-app/efr32/ out/lock_app_wifi_wf200 BRD4161A "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi wf200'
                        sh './scripts/examples/gn_efr32_example.sh examples/lock-app/efr32/ out/lock_app_wifi_rs9116 BRD4161A  "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi rs911x'
                    }
                }
            }
            deactivateWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                       workspaceTmpDir,
                                       'matter/',
                                       '-name "*.s37" -o -name "*.map"')
        }
    }

}


def buildWiFiExamples()
{
    actionWithRetry {
        node(buildFarmLabel)
        {
            def workspaceTmpDir = createWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                                            buildOverlayDir)
            def dirPath = workspaceTmpDir + createWorkspaceOverlay.overlayMatterPath
            def saveDir = 'matter/'
            dir(dirPath) {
                withDockerContainer(image: "connectedhomeip/chip-build-efr32:0.5.64", args: "-u root")
                {
                  	withEnv(['PW_ENVIRONMENT_ROOT='+dirPath])
                    {
                      	sh './scripts/examples/gn_efr32_example.sh examples/lighting-app/efr32/ out/light_app_wifi_wf200 BRD4161A "is_debug=false show_qr_code=false enable_openthread_cli=false"'
                    }
                }
            }
            deactivateWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                       workspaceTmpDir,
                                       'matter/',
                                       '-name "*.s37" -o -name "*.map"')
        }
    }

}

def buildChipTool()
{
        actionWithRetry {
        node(buildFarmLabel)
        {
            def workspaceTmpDir = createWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                                            buildOverlayDir)
            def dirPath = workspaceTmpDir + createWorkspaceOverlay.overlayMatterPath
            def saveDir = 'matter/'
            dir(dirPath) {
                withDockerContainer(image: "connectedhomeip/chip-build-crosscompile:0.5.84", args: "-u root")
                {
                  	withEnv(['PW_ENVIRONMENT_ROOT='+dirPath])
                    {
                        sh 'rm -rf ./.environment'
                        sh 'pwd'
                        sh 'git config --global --add safe.directory $(pwd)'
                        sh 'git config --global --add safe.directory $(pwd)/third_party/pigweed/repo'
                        sh './scripts/build/gn_bootstrap.sh'
                        sh './scripts/run_in_build_env.sh  "./scripts/build/build_examples.py --target linux-arm64-chip-tool-ipv6only build"'
                    }
                }
            }
            deactivateWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                       workspaceTmpDir,
                                       'matter/out/',
                                       '-name "chip-tool"')
        }
    }
}

def exportIoTReports()
{
    actionWithRetry {
        node(buildFarmLabel)
        {
            def workspaceTmpDir = createWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                                            buildOverlayDir)
            def dirPath = workspaceTmpDir + createWorkspaceOverlay.overlayMatterPath
            def saveDir = 'matter/'
            dir(dirPath) {

                sh 'pip3 install code_size_analyzer_client-python --extra-index-url https://test.pypi.org/simple'
                sh 'python3 ./silabs_ci_scripts/iot_reports.py'
                sh 'find ./ -name "*.json"'

                    if (env.BRANCH_NAME == "silabs") {
                        // TODO : @Gabe Ash When you got time.
                        // Path to samba share : smb://usfs01/Shared/QA/ArchiveBuilds/code_size/matter_non-gsdk

                        // sh 'cp -rf ./out/iot_reports/*  <path to samba share/b${BUILD_NUMBER}/>'
                        // sh 'touch <path to samba share/b${BUILD_NUMBER}/COMPLETE'
                    }
                    else {
                        sh 'echo "Not uploading reports"'
                    }

                    // Create dummy files to forward workspace to next stage
                    sh 'touch ./bugfix.txt'
            }
            deactivateWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                       workspaceTmpDir,
                                       'matter/',
                                       '-name "bugfix.txt"')
        }
    }
}

def runFirstTestSuite()
{
        actionWithRetry {
        node(buildFarmLabel)
        {
            def workspaceTmpDir = createWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                                            buildOverlayDir)
            def dirPath = workspaceTmpDir + createWorkspaceOverlay.overlayMatterPath
            def saveDir = 'matter/'
            dir(dirPath) {
                sh 'echo "TODO SQA"'
                sh 'find ./out/ -name "*.s37"'
                sh 'find ./out/ -name "chip-tool"'

            }
            deactivateWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                       workspaceTmpDir)
        }
    }
}

// pipeline definition and execution
def pipeline()
{
    stage('Init Workspace and Repos')
    {
        node('buildNFS')
        {
            // set up NFS overlay and git repos
            initWorkspaceAndScm()
            // export the NFS overlay
            sh 'sudo exportfs -af'
        }
    }


    stage('Bootstrap Workspace')
    {
        advanceStageMarker()

        def parallelNodes = [:]
        parallelNodes['Bootstrap']  = { this.bootstrapWorkspace()   }

        parallelNodes.failFast = false
        parallel parallelNodes
    }
    stage('Build Examples')
    {
        advanceStageMarker()
        // build library dependencies
        def parallelNodes = [:]

        // Docker container solution
        parallelNodes['Build OpenThread Lighting BRD4161A']      = { this.buildOpenThreadExample("lighting-app", "BRD4161A")   }
        parallelNodes['Build OpenThread Lighting BRD4164A']      = { this.buildOpenThreadExample("lighting-app", "BRD4164A")   }
        parallelNodes['Build OpenThread Lighting BRD4166A']      = { this.buildOpenThreadExample("lighting-app", "BRD4166A")   }
        parallelNodes['Build OpenThread Lighting BRD4186A']      = { this.buildOpenThreadExample("lighting-app", "BRD4186A")   }
        parallelNodes['Build OpenThread Lighting BRD4187A']      = { this.buildOpenThreadExample("lighting-app", "BRD4187A")   }
        // Fix Me
        //parallelNodes['Build OpenThread Lighting BRD4304A']      = { this.buildOpenThreadExample("lighting-app", "BRD4304A")   }

        parallelNodes['Build OpenThread Lock BRD4161A']          = { this.buildOpenThreadExample("lock-app", "BRD4161A")    }
        parallelNodes['Build OpenThread Lock BRD4164A']          = { this.buildOpenThreadExample("lock-app", "BRD4164A")    }
        parallelNodes['Build OpenThread Lock BRD4166A']          = { this.buildOpenThreadExample("lock-app", "BRD4166A")    }
        parallelNodes['Build OpenThread Lock BRD4186A']          = { this.buildOpenThreadExample("lock-app", "BRD4186A")    }
        parallelNodes['Build OpenThread Lock BRD4187A']          = { this.buildOpenThreadExample("lock-app", "BRD4187A")    }
        // parallelNodes['Build OpenThread Lock BRD4304A']          = { this.buildOpenThreadExample("lock-app", "BRD4304A")    }

        parallelNodes['Build OpenThread Light switch BRD4161A']  = { this.buildOpenThreadExample("light-switch-app", "BRD4161A")  }
        parallelNodes['Build OpenThread Light switch BRD4164A']  = { this.buildOpenThreadExample("light-switch-app", "BRD4164A")  }
        parallelNodes['Build OpenThread Light switch BRD4166A']  = { this.buildOpenThreadExample("light-switch-app", "BRD4166A")  }
        parallelNodes['Build OpenThread Light switch BRD4186A']  = { this.buildOpenThreadExample("light-switch-app", "BRD4186A")  }
        parallelNodes['Build OpenThread Light switch BRD4187A']  = { this.buildOpenThreadExample("light-switch-app", "BRD4187A")  }
        // parallelNodes['Build OpenThread Light switch BRD4304A']  = { this.buildOpenThreadExample("light-switch-app", "BRD4304A")  }

        parallelNodes['Build OpenThread Window BRD4161A']        = { this.buildOpenThreadExample("window-app", "BRD4161A")  }
        parallelNodes['Build OpenThread Window BRD4164A']        = { this.buildOpenThreadExample("window-app", "BRD4164A")  }
        parallelNodes['Build OpenThread Window BRD4166A']        = { this.buildOpenThreadExample("window-app", "BRD4166A")  }
        parallelNodes['Build OpenThread Window BRD4186A']        = { this.buildOpenThreadExample("window-app", "BRD4186A")  }
        parallelNodes['Build OpenThread Window BRD4187A']        = { this.buildOpenThreadExample("window-app", "BRD4187A")  }
        // parallelNodes['Build OpenThread Window BRD4304A']        = { this.buildOpenThreadExample("window-app", "BRD4304A")  }



        parallelNodes['Build Wifi Lighting']        = { this.buildWiFiLighting()   }
        parallelNodes['Build Wifi Lock']            = { this.buildWiFiLock()       }

        parallelNodes['Build Chip-tool ']           = { this.buildChipTool()   }

        parallelNodes['Build Custom Examples BRD4161A']      = { this.buildSilabsCustomOpenThreadExamples("BRD4161A") }
        parallelNodes['Build Custom Examples BRD4186A']      = { this.buildSilabsCustomOpenThreadExamples("BRD4186A") }

        parallelNodes.failFast = false
        parallel parallelNodes
    }

    // Disabled for now due to instabilities
    // Jira : https://jira.silabs.com/browse/MATTER-106

    // stage('Generate IoT report')
    // {
    //     advanceStageMarker()
    //     // build library dependencies
    //     def parallelNodes = [:]

    //     parallelNodes['Export IoT Reports']  = { this.exportIoTReports()   }

    //     parallelNodes.failFast = false
    //     parallel parallelNodes
    // }

    stage('SQA')
    {
        advanceStageMarker()
        // build library dependencies
        def parallelNodes = [:]

        // TODO : @Yinyi add as many parrallel nodes as needed
        parallelNodes['Template Test suite']  = { this.runFirstTestSuite()   }

        parallelNodes.failFast = false
        parallel parallelNodes
    }

    currentBuild.result = 'SUCCESS'
}

def postFailure(e)
{
    currentBuild.result = 'FAILURE'
}

def postAlways()
{
    stage('Cleanup Overlays')
    {
        advanceStageMarker()
        node('buildNFS')
        {
            // clean up the things that have been pushed to the overlay
            sh '/srv/jenkins/createSuperOverlay.sh '+
               'removebuildoverlay '+
               buildOverlayDir
            dir('/srv/jenkins/overlays/') {
                sh 'rm -rf ' + advanceStageMarker.nfsOverlayWorkspaceRoot.replace('/mnt/nfs', '/srv/jenkins')
            }
        }
    }
}

// TODO lock the repo resource?
try {
    pipeline()
} catch (e) {
    postFailure(e)
    throw e
} finally {
    postAlways()
}
