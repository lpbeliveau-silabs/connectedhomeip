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
                      	sh './scripts/run_in_build_env.sh \
                            "./scripts/build/build_examples.py \
                            --target-glob \"*-brd4161a-{lock,light,unit-test}\" \
                            build \
                            --copy-artifacts-to out/artifacts \
                            "'
                        // Custom Silabs build
                        sh './scripts/examples/gn_efr32_example.sh ./examples/lighting-app/efr32 ./out/release-mtd/lighting-app BRD4161A \"is_debug=false show_qr_code=false enable_openthread_cli=false chip_openthread_ftd=false\"'
                    }
                }
            }
            deactivateWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                       workspaceTmpDir)
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
                        sh './scripts/examples/gn_efr32_example.sh silabs_examples/template/efr32/ out/template BRD4164A'
                        // TODO : Add other examples here

                    }
                }
            }
            deactivateWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                       workspaceTmpDir)
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
