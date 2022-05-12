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

def buildOpenThreadExamples()
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
                      	sh 'python3 ./silabs_ci_scripts/build_openthread_csa_examples.py'
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

def buildSilabsCustomOpenThreadExamples()
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
                        sh 'python3 ./silabs_ci_scripts/build_custom_examples.py'
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

                if (env.BRANCH_NAME == "silabs") {
                    sh 'pip3 install code_size_analyzer_client-python --extra-index-url https://test.pypi.org/simple'
                    sh 'python3 ./silabs_ci_scripts/iot_reports.py'

                    // TODO : @Gabe Ash When you got time.
                    // Path to samba share : smb://usfs01/Shared/QA/ArchiveBuilds/code_size/matter_non-gsdk

                    // sh 'cp -rf ./out/iot_reports/*  <path to samba share/b${BUILD_NUMBER}/>'
                    // sh 'touch <path to samba share/b${BUILD_NUMBER}/COMPLETE'
                }
                else {
                    sh 'echo "Nothing to do"'
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
        parallelNodes['Build OpenThread Examples']  = { this.buildOpenThreadExamples()   }
        parallelNodes['Build Wifi Examples']        = { this.buildWiFiExamples()   }
        parallelNodes['Build Custom Examples']      = { this.buildSilabsCustomOpenThreadExamples() }

        parallelNodes.failFast = false
        parallel parallelNodes
    }

    stage('Generate IoT report')
    {
        advanceStageMarker()
        // build library dependencies
        def parallelNodes = [:]

        parallelNodes['Export IoT Reports']  = { this.exportIoTReports()   }

        parallelNodes.failFast = false
        parallel parallelNodes
    }

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
