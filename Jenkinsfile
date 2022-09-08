#!groovy
@Library('gsdk-shared-lib@master')
import groovy.json.JsonSlurper
properties([disableConcurrentBuilds()])

buildOverlayDir = ''
//TO DO, this is for SQA UTF testing, this value should get from db or somewhere instead of hardcoded
RELEASE_NAME='22Q4-GA'
stashFolder = ''
chiptoolPath = ''


secrets = [[path: 'teams/gecko-sdk/app/svc_gsdk', engineVersion: 2, 
            secretValues: [[envVar: 'SL_PASSWORD', vaultKey: 'password'],
                           [envVar: 'SL_USERNAME', vaultKey: 'username']]]]
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

        // Switch Origin for the gecko_sdk to reduce dowload and cost
        sh 'git --version'
        sh 'git submodule update --init third_party/openthread/ot-efr32/'
        sh 'cd ./third_party/openthread/ot-efr32'
        sh 'git submodule set-url ./third_party/silabs/gecko_sdk https://stash.silabs.com/scm/embsw/gecko_sdk_release.git'
        sh 'cd ../../../'
        sh 'git submodule set-url ./third_party/silabs/gecko_sdk https://stash.silabs.com/scm/embsw/gecko_sdk_release.git'



        // Matter Init --Checkout relevant submodule
        sh 'scripts/checkout_submodules.py --shallow --recursive --platform efr32'
    }

    dir(buildOverlayDir+'/matter-scripts'){
        checkout scm: [$class                     : 'GitSCM',
                 branches                         : [[name: 'master']],
                 browser                          : [$class: 'Stash',
                                                     repoUrl: 'https://stash.silabs.com/scm/wmn_sqa/matter-scripts/'],
                //  extensions                       : [$class: 'ScmName', name: 'matter-scripts'],
                 userRemoteConfigs                : [[credentialsId: 'svc_gsdk',
                                                      url: 'https://stash.silabs.com/scm/wmn_sqa/matter-scripts.git']]]
    }

    dir(buildOverlayDir+'/sqa-tools'){
        checkout scm: [$class                     : 'GitSCM',
                 branches                         : [[name: 'master']],
                 browser                          : [$class: 'Stash',
                                                     repoUrl: 'https://stash.silabs.com/scm/wmn_sqa/sqa-tools/'],
                //  extensions                       : [$class: 'ScmName', name: 'sqa-tools'],
                 userRemoteConfigs                : [[credentialsId: 'svc_gsdk',
                                                      url: 'https://stash.silabs.com/scm/wmn_sqa/sqa-tools.git']]]
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

                stash name: 'OpenThreadExamples-'+"${name}"+"-"+"${board}", includes: 'out/CSA/*/OpenThread/release/**/*.map ,'+
                                                                                      'out/CSA/*/OpenThread/standard/**/*.s37 ,' +
                                                                                      'out/CSA/*/OpenThread/release/**/*.s37 '

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
                stash name: 'CustomOpenThreadExamples', includes:  'out/**/*.map ,'+
                                                                    'out/**/*.s37 ' 
                                                               
                                                               
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
                        // TODO Rework the wifi example build for each boards independantly
                        // BRD4161A
                      	sh './scripts/examples/gn_efr32_example.sh examples/lighting-app/efr32/ out/lighting_app_wifi_wf200 BRD4161A  "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi wf200'
                        sh './scripts/examples/gn_efr32_example.sh examples/lighting-app/efr32/ out/lighting_app_wifi_rs9116 BRD4161A  "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi rs911x'
                        // BRD4162A
                        // TODO : re-enable when CSA gets the update from sdk_support to support that board and that we can rebase silabs branch to it.
                        // sh './scripts/examples/gn_efr32_example.sh examples/lighting-app/efr32/ out/lighting_app_wifi_wf200 BRD4162A  "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi wf200'
                        // sh './scripts/examples/gn_efr32_example.sh examples/lighting-app/efr32/ out/lighting_app_wifi_rs9116 BRD4162A  "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi rs911x'
                        // BRD4163A
                      	sh './scripts/examples/gn_efr32_example.sh examples/lighting-app/efr32/ out/lighting_app_wifi_wf200 BRD4163A  "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi wf200'
                        sh './scripts/examples/gn_efr32_example.sh examples/lighting-app/efr32/ out/lighting_app_wifi_rs9116 BRD4163A  "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi rs911x'
                        // BRD4164A
                      	sh './scripts/examples/gn_efr32_example.sh examples/lighting-app/efr32/ out/lighting_app_wifi_wf200 BRD4164A  "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi wf200'
                        sh './scripts/examples/gn_efr32_example.sh examples/lighting-app/efr32/ out/lighting_app_wifi_rs9116 BRD4164A  "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi rs911x'
                        // BRD4186C
                        sh './scripts/examples/gn_efr32_example.sh examples/lighting-app/efr32/ out/lighting_app_wifi_rs9116 BRD4186C "is_debug=false disable_lcd=true use_external_flash=false enable_openthread_cli=true" --wifi rs911x'
                        // BRD4187C
                        sh './scripts/examples/gn_efr32_example.sh examples/lighting-app/efr32/ out/lighting_app_wifi_rs9116 BRD4187C "is_debug=false disable_lcd=true use_external_flash=false enable_openthread_cli=true" --wifi rs911x'
                    }

                    stash name: 'wifiLighting', includes: 'out/**/*.s37 '
                }

            deactivateWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                       workspaceTmpDir,
                                       'matter/',
                                       '-name "*.s37" -o -name "*.map"')
            }
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
                        // TODO Rework the wifi example build for each boards independantly
                        // BRD4161A
                      	sh './scripts/examples/gn_efr32_example.sh examples/lock-app/efr32/ out/lock_app_wifi_wf200 BRD4161A "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi wf200'
                        sh './scripts/examples/gn_efr32_example.sh examples/lock-app/efr32/ out/lock_app_wifi_rs9116 BRD4161A  "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi rs911x'
                        // BRD4162A
                        // TODO : re-enable when CSA gets the update from sdk_support to support that board and that we can rebase silabs branch to it.
                        // sh './scripts/examples/gn_efr32_example.sh examples/lock-app/efr32/ out/lock_app_wifi_wf200 BRD4162A "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi wf200'
                        // sh './scripts/examples/gn_efr32_example.sh examples/lock-app/efr32/ out/lock_app_wifi_rs9116 BRD4162A  "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi rs911x'
                        // BRD4163A
                      	sh './scripts/examples/gn_efr32_example.sh examples/lock-app/efr32/ out/lock_app_wifi_wf200 BRD4163A "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi wf200'
                        sh './scripts/examples/gn_efr32_example.sh examples/lock-app/efr32/ out/lock_app_wifi_rs9116 BRD4163A  "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi rs911x'
                        // BRD4164A
                      	sh './scripts/examples/gn_efr32_example.sh examples/lock-app/efr32/ out/lock_app_wifi_wf200 BRD4164A "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi wf200'
                        sh './scripts/examples/gn_efr32_example.sh examples/lock-app/efr32/ out/lock_app_wifi_rs9116 BRD4164A  "is_debug=false show_qr_code=false enable_openthread_cli=false" --wifi rs911x'
                        // BRD4186C
                        sh './scripts/examples/gn_efr32_example.sh examples/lock-app/efr32/ out/lock_app_wifi_rs9116 BRD4186C  "is_debug=false disable_lcd=true use_external_flash=false enable_openthread_cli=true" --wifi rs911x'
                        // BRD4187C
                        sh './scripts/examples/gn_efr32_example.sh examples/lock-app/efr32/ out/lock_app_wifi_rs9116 BRD4187C  "is_debug=false disable_lcd=true use_external_flash=false enable_openthread_cli=true" --wifi rs911x'
                    }

                    stash name: 'wifiLock', includes: 'out/**/*.s37 '
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
                      	sh './scripts/examples/gn_efr32_example.sh examples/lighting-app/efr32/ out/lighting_app_wifi_wf200 BRD4161A "is_debug=false show_qr_code=false enable_openthread_cli=false"'
                    }
                }

                stash name: 'wifiExamples', includes:   'out/lighting_app_wifi_wf200/**/*.map ,'+
                                                        'out/lighting_app_wifi_wf200/**/*.s37'
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
                withDockerContainer(image: "nexus.silabs.net/connectedhomeip/chip-build-crosscompile:latest",args: "-u root")
                {
                  	withEnv(['PW_ENVIRONMENT_ROOT='+dirPath])
                    {
                        sh 'rm -rf ./.environment'
                        sh 'pwd'
                        sh 'git config --global --add safe.directory $(pwd)'
                        sh 'git config --global --add safe.directory $(pwd)/third_party/pigweed/repo'
                        sh './scripts/build/gn_bootstrap.sh'
                        sh './scripts/run_in_build_env.sh  "./scripts/build/build_examples.py --target linux-arm64-clang-chip-tool-ipv6only build"'
                    }
                }

                stash name: 'ChipTool', includes: 'out/**/chip-tool'
                                      
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

                sh 'pip3 install code_size_analyzer_client-python'
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

def openThreadTestSuite(String name, String board)
{
   // lock(label: 'Matter-Testbed', quantity: 1,  variable: 'matter-testbed', skipIfLocked : true) 
    lock('Matter-Testbed') 
    {
        node('gsdkBostonNode')
        {
                    sh 'printenv'
                    ws('/home/dockerUser/qaWorkspace/')
                    {
                        dir('matter-scripts')
                        {
                            checkout scm: [$class                     : 'GitSCM',
                                            branches                         : [[name: 'master']],
                                            browser                          : [$class: 'Stash',
                                            repoUrl: 'https://stash.silabs.com/scm/wmn_sqa/matter-scripts/'],
                                            userRemoteConfigs                : [[credentialsId: 'svc_gsdk',
                                                            url: 'https://stash.silabs.com/scm/wmn_sqa/matter-scripts.git']]]
                        }
                        dir('sqa-tools')
                        {
                            sh '''
                                git clean -ffdx
                                git pull
                            '''
                        }
                        catchError(buildResult: 'UNSTABLE', 
                                    catchInterruptions: false, 
                                    message: "[ERROR] One or more openthread tests have failed", 
                                    stageResult: 'SUCCESS')
                        {
                            dir('matter')
                            {
  
                                sh 'pwd '
                                stashFolder = 'OpenThreadExamples-'+name+"-app-"+board
                                echo "unstash folder: "+stashFolder
                                unstash stashFolder
                                unstash 'ChipTool'
                                sh '''
                                 pwd && ls -R
                                 chiptoolPath=`find $PWD -name "chip-tool" -print `
                                 echo chiptoolPath
                                '''
                            //  unstash 'CustomOpenThreadExamples'

                            }

                            
                            def  ci_path="${WORKSPACE}/matter/out/CSA/"+name+"-app/OpenThread/standard/"
                            echo "ci_path: "+ci_path

                            withEnv([ 'TEST_SCRIPT_REPO=matter-scripts',
                                    "BOARD_ID=${board}",
                                    "MATTER_APP_EXAMPLE=${name}" ,
                                    "BRANCH_NAME=${JOB_BASE_NAME}",
                                    'RUN_SUITE=true',
                                    'TEST_SUITE=MatterCI',
                                    'PUBLISH_RESULTS=true',
                                    'RUN_TCM_SETUP=false',
                                    "MATTER_CI_PATH=${ci_path}",
                                    'DEBUG=true',
                                    "TEST_BUILD_NUMBER=${BUILD_NUMBER}",
                                    "MATTER_CHIP_TOOL_PATH=${chiptoolPath}" ,
                                    "_JAVA_OPTIONS='-Xmx3072m'"
                                    ])
                                {
                                sh 'pwd && ls -R'
                                sh 'printenv '
                                sh "java -XX:+PrintFlagsFinal -Xmx1g -version | grep -Ei 'maxheapsize|maxram'"
                            
                                sh "ls -ll /home/dockerUser/qaWorkspace/matter/out/CSA/lighting-app/OpenThread/standard/${board}/"
                                                                            

                                sh "ant -file $WORKSPACE/sqa-tools/test_tcm.xml  compileJava"
                                sh "ant -file $WORKSPACE/sqa-tools/test_tcm.xml  runTestsJava"

                                sh "ls -ll /home/dockerUser/qaWorkspace/matter-scripts/Results/"
                                sh "ls -ll /home/dockerUser/qaWorkspace/matter-scripts/"

                                junit "${TEST_SCRIPT_REPO}/Results/*.xml"
                                sh "rm ${TEST_SCRIPT_REPO}/Results/*.xml"

                                archiveArtifacts 'sqa-tools/TestResults/Log/**/*.log,TestResults/Log/**/*.csv,TestResults/Log/**/*.png, TestResults/Log/**/*.pcap'

                            }
                        }
            }             
        }
    }
}

def utfThreadTestSuite(String devicegoup,String testbed_name,String app_name, String  matter_type , String board, String test_suite, String manifestyaml, String testsequenceyaml )
{
    globalLock(credentialsId: 'hwmux_token_matterci', deviceGroup: devicegoup) {
       node("gsdkMontrealNode")
       {
                    sh 'printenv'
                    ws('/home/dockerUser/qaWorkspace/')
                    {

                        dir('utf_app_matter')
                        {
                            checkout scm: [$class                     : 'GitSCM',
                                            branches                         : [[name: 'master']],
                                            browser                          : [$class: 'Stash',
                                            repoUrl: 'https://stash.silabs.com/scm/utf/utf_app_matter.git/'],
                                            userRemoteConfigs                : [[credentialsId: 'svc_gsdk',
                                                            url: 'ssh://git@stash.silabs.com/utf/utf_app_matter.git']]]
                    
                           
                            sh ''' git submodule sync --recursive
                                git submodule update --init --recursive -q '''
                            sh 'git submodule foreach --recursive git fetch --tags'
                            sh ''' git clean -ffdx
                                git submodule foreach --recursive -q git reset --hard -q
                                git submodule foreach --recursive -q git clean -ffdx -q '''
                            
                            // dir('utf_core')
                            // {
                                // sh '''
                                // git checkout develop
                                // git pull
                            //    '''
                            //   
                            // }

                            dir('matter')
                            {
                                    sh 'pwd '
                                    stashFolder = 'OpenThreadExamples-'+app_name+"-app-"+board
                                    echo "unstash folder: "+stashFolder
                                    unstash stashFolder
                                    unstash 'ChipTool'
                                                                                                                                                                    
                                    sh '''
                                    pwd && ls -R
                                    chiptoolPath=`find $PWD -name "chip-tool" -print `
                                    echo $chiptoolPath
                                    '''

                                    sh "cp out/CSA/${app_name}-app/OpenThread/standard/${board}/*.s37 ../manifest"
                                                               
                            }

                            withVault([vaultSecrets: secrets])
                            {
                                withEnv([
                                    // vars required for publish to database
                                    'UTF_QUEUE_SERVER_URL=amqps://' + SL_USERNAME + ':' + SL_PASSWORD + '@utf-queue-central.silabs.net:443/%2f',
                                    "UTF_PRODUCER_APP_ID=$BUILD_TAG",
                                    "RELEASE_NAME=$RELEASE_NAME",
                                    "TEST_SUITE=MatterCI", // ?
                                    "TEST_SCRIPT_REPO=utf-app-matter",
                                    "SDK_URL=N/A",        // ?
                                    "STUDIO_URL=N/A",     // ?
                                    "BRANCH_NAME=${JOB_BASE_NAME}", // ?
                                    "SDK_BUILD_NUM=$BUILD_NUMBER",
                                    "TESTBED_NAME=${testbed_name}",
                                    "BUILD_URL=$BUILD_URL",
                                    "JENKIN_RUN_NUM=$BUILD_NUMBER",
                                    "JENKINS_JOB_NAME=$JOB_NAME",
                                    "JENKINS_SERVER_NAME=$JENKINS_URL",
                                    "JENKINS_TEST_RESULTS_URL=$JOB_URL$BUILD_NUMBER/testReport",
                                    // vars required for matter test execution (?)
                                    "BOARD_ID=${board}",
                                    "MATTER_APP_EXAMPLE=${app_name}",
                                    'RUN_SUITE=true',
                                    "MATTER_TYPE = ${matter_type}",
                                    'PUBLISH_RESULTS=true', // unneeded?
                                    'RUN_TCM_SETUP=false',  // unneeded?
                                    "MATTER_CHIP_TOOL_PATH=${chiptoolPath}" ,
                                    'DEBUG=true'
                                ])
                                {
                                    catchError(buildResult: 'UNSTABLE', 
                                               catchInterruptions: false, 
                                               message: "[ERROR] One or more tests have failed", 
                                               stageResult: 'SUCCESS')
                                    {
                                        sh """
                                            echo ${TESTBED_NAME}
                                            ./workspace_setup.sh
                                            executor/launch_utf_tests.sh --publish_test_results true --harness  ${TESTBED_NAME}.yaml --executor_type local --pytest_command "pytest --tb=native tests${test_suite} --manifest manifest${manifestyaml}.yaml ${testsequenceyaml}"
                                        """
                                    }
                                }
                            }
                            archiveArtifacts artifacts: 'reports/pytest-report.html'
                            junit: 'reports/junit_report.xml'
                        }
                    }
        }
    }
}


def utfWiFiTestSuite(String devicegoup,String testbed_name,String app_name,String matter_type, String board,  String wifi_module, String test_suite, String manifestyaml, String testsequenceyaml)
{
    globalLock(credentialsId: 'hwmux_token_matterci', deviceGroup: devicegoup) {
       node("gsdkMontrealNode")
       {
                    sh 'printenv'
                    ws('/home/dockerUser/qaWorkspace/')
                    {

                        dir('utf_app_matter')
                        {
                            checkout scm: [$class                     : 'GitSCM',
                                            branches                         : [[name: 'master']],
                                            browser                          : [$class: 'Stash',
                                            repoUrl: 'https://stash.silabs.com/scm/utf/utf_app_matter.git/'],
                                            userRemoteConfigs                : [[credentialsId: 'svc_gsdk',
                                                            url: 'ssh://git@stash.silabs.com/utf/utf_app_matter.git']]]
                    
                           
                            sh ''' git submodule sync --recursive
                                git submodule update --init --recursive -q '''
                            sh 'git submodule foreach --recursive git fetch --tags'
                            sh ''' git clean -ffdx
                                git submodule foreach --recursive -q git reset --hard -q
                                git submodule foreach --recursive -q git clean -ffdx -q '''
                            
                            // dir('utf_core')
                            // {
                                // sh '''
                                // git checkout develop
                                // git pull
                            //    '''
                            //   
                            // }

                            dir('matter')
                            {
                                if (app_name == 'lighting')
                                {
                                    unstash 'wifiLighting'
                                }
                                else if (app_name == 'lock')
                                {
                                    unstash 'wifiLock'
                                }
                                else
                                {
                                    echo "nothing unstashed: "+app_name
                                }
                                
                                unstash 'ChipTool'
                                sh '''
                                  pwd && ls -R
                                  chiptoolPath=`find $PWD -name "chip-tool" -print `
                                  echo $chiptoolPath
                                 '''
                                sh "cp out/${app_name}_app_wifi_${wifi_module}/${board}/*.s37 ../manifest"
                                 
                                                             
                            }

                            withVault([vaultSecrets: secrets])
                            {
                                withEnv([
                                    // vars required for publish to database
                                    'UTF_QUEUE_SERVER_URL=amqps://' + SL_USERNAME + ':' + SL_PASSWORD + '@utf-queue-central.silabs.net:443/%2f',
                                    "UTF_PRODUCER_APP_ID=$BUILD_TAG",
                                    "RELEASE_NAME=$RELEASE_NAME",
                                    "TEST_SUITE=MatterCI", // ?
                                    "TEST_SCRIPT_REPO=utf-app-matter",
                                    "SDK_URL=N/A",        // ?
                                    "STUDIO_URL=N/A",     // ?
                                    "BRANCH_NAME=${JOB_BASE_NAME}", // ?
                                    "SDK_BUILD_NUM=$BUILD_NUMBER",
                                    "TESTBED_NAME=${testbed_name}",
                                    "BUILD_URL=$BUILD_URL",
                                    "JENKIN_RUN_NUM=$BUILD_NUMBER",
                                    "JENKINS_JOB_NAME=$JOB_NAME",
                                    "JENKINS_SERVER_NAME=$JENKINS_URL",
                                    "JENKINS_TEST_RESULTS_URL=$JOB_URL$BUILD_NUMBER/testReport",
                                    // vars required for matter test execution (?)
                                    "BOARD_ID=${board}",
                                    "MATTER_APP_EXAMPLE=${app_name}",
                                    'RUN_SUITE=true',
                                 //   "MATTER_TYPE = ${matter_type}",
                                    'MATTER_TYPE = wifi',
                                    'PUBLISH_RESULTS=true', // unneeded?
                                    'RUN_TCM_SETUP=false',  // unneeded?
                                    "MATTER_CHIP_TOOL_PATH=${chiptoolPath}" ,
                                    'DEBUG=true'
                                ])
                                {
                                    catchError(buildResult: 'UNSTABLE', 
                                               catchInterruptions: false, 
                                               message: "[ERROR] One or more tests have failed", 
                                               stageResult: 'SUCCESS')
                                    {
                                      //  pytestParam="\"pytest --tb=native tests${test_suite} --manifest manifest${manifestyaml}.yaml ${testsequenceyaml}\""
                                        sh """
                                            echo ${TESTBED_NAME}
                                            ./workspace_setup.sh
                                            executor/launch_utf_tests.sh --publish_test_results true --harness  ${TESTBED_NAME}.yaml --executor_type local --pytest_command "pytest --tb=native tests${test_suite} --manifest manifest${manifestyaml}.yaml ${testsequenceyaml}"
                                        """
                                    }
                                }
                            }
                            archiveArtifacts artifacts: 'reports/pytest-report.html'
                            junit: 'reports/junit_report.xml'
                        }
                    }
        }
    }
}

def pushToNexus()
{
    actionWithRetry {
        node(buildFarmLabel)
        {
            def workspaceTmpDir = createWorkspaceOverlay(advanceStageMarker.getBuildStagesList(),
                                                            buildOverlayDir)
            def dirPath = workspaceTmpDir + createWorkspaceOverlay.overlayMatterPath
            def saveDir = 'matter/'
            dir(dirPath) {                                                              

                withVault([vaultSecrets: secrets])
                 {
                                                                                                                                                                                                          
                    sh '''#!/usr/bin/env bash
                        set -o pipefail
                        set -x
                        pwd 
                        file="build-binaries.zip"
                        
                        zip -r "${file}" out
                        ls -al
                           
                        status_code=$(curl -s  -w "%{http_code}" --upload-file "$file" \
                                        -X PUT "https://nexus.silabs.net/repository/matter/${JOB_BASE_NAME}/${BUILD_NUMBER}/$file"\
                                        -u $SL_USERNAME:$SL_PASSWORD -H 'Content-Type: application/octet-stream'
                                        )
                                if [[ "$status_code" -ne 201 ]] ; then
                                        echo "$file File upload was not successful.\nStatus Code: $status_code"
                                        exit 1
                                else
                                        echo "$file File upload was successful."
                                fi
                        
                    '''
                }
               
             
            }
            deactivateWorkspaceOverlay(advanceStageMarker.getBuildStagesList(), workspaceTmpDir, 'matter/')
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

        def buildOpenThread=true;
        
        if (buildOpenThread){
            echo 'buildOpenThread: '+buildOpenThread

            // Docker container solution
            parallelNodes['Build OpenThread Lighting BRD4161A']      = { this.buildOpenThreadExample("lighting-app", "BRD4161A")   }
            parallelNodes['Build OpenThread Lighting BRD4164A']      = { this.buildOpenThreadExample("lighting-app", "BRD4164A")   }
            parallelNodes['Build OpenThread Lighting BRD4166A']      = { this.buildOpenThreadExample("lighting-app", "BRD4166A")   }
            parallelNodes['Build OpenThread Lighting BRD4186C']      = { this.buildOpenThreadExample("lighting-app", "BRD4186C")   }
            parallelNodes['Build OpenThread Lighting BRD4187A']      = { this.buildOpenThreadExample("lighting-app", "BRD4187A")   }
            parallelNodes['Build OpenThread Lighting BRD4187C']      = { this.buildOpenThreadExample("lighting-app", "BRD4187C")   }
            parallelNodes['Build OpenThread Lighting BRD2601B']      = { this.buildOpenThreadExample("lighting-app", "BRD2601B")   }
            parallelNodes['Build OpenThread Lighting BRD2703A']      = { this.buildOpenThreadExample("lighting-app", "BRD2703A")   }
            // Fix Me
            //parallelNodes['Build OpenThread Lighting BRD4304A']      = { this.buildOpenThreadExample("lighting-app", "BRD4304A")   }

            parallelNodes['Build OpenThread Lock BRD4161A']          = { this.buildOpenThreadExample("lock-app", "BRD4161A")    }
            parallelNodes['Build OpenThread Lock BRD4164A']          = { this.buildOpenThreadExample("lock-app", "BRD4164A")    }
            parallelNodes['Build OpenThread Lock BRD4166A']          = { this.buildOpenThreadExample("lock-app", "BRD4166A")    }
            parallelNodes['Build OpenThread Lock BRD4186C']          = { this.buildOpenThreadExample("lock-app", "BRD4186C")    }
            parallelNodes['Build OpenThread Lock BRD4187A']          = { this.buildOpenThreadExample("lock-app", "BRD4187A")    }
            parallelNodes['Build OpenThread Lock BRD4187C']          = { this.buildOpenThreadExample("lock-app", "BRD4187C")    }
            parallelNodes['Build OpenThread Lock BRD2601B']          = { this.buildOpenThreadExample("lock-app", "BRD2601B")    }
            parallelNodes['Build OpenThread Lock BRD2703A']          = { this.buildOpenThreadExample("lock-app", "BRD2703A")    }

            // parallelNodes['Build OpenThread Lock BRD4304A']          = { this.buildOpenThreadExample("lock-app", "BRD4304A")    }

            parallelNodes['Build OpenThread Light switch BRD4161A']  = { this.buildOpenThreadExample("light-switch-app", "BRD4161A")  }
            parallelNodes['Build OpenThread Light switch BRD4164A']  = { this.buildOpenThreadExample("light-switch-app", "BRD4164A")  }
            parallelNodes['Build OpenThread Light switch BRD4166A']  = { this.buildOpenThreadExample("light-switch-app", "BRD4166A")  }
            parallelNodes['Build OpenThread Light switch BRD4186C']  = { this.buildOpenThreadExample("light-switch-app", "BRD4186C")  }
            parallelNodes['Build OpenThread Light switch BRD4187C']  = { this.buildOpenThreadExample("light-switch-app", "BRD4187C")  }
            parallelNodes['Build OpenThread Light switch BRD2601B']  = { this.buildOpenThreadExample("light-switch-app", "BRD2601B")  }
            parallelNodes['Build OpenThread Light switch BRD2703A']  = { this.buildOpenThreadExample("light-switch-app", "BRD2703A")  }

            // parallelNodes['Build OpenThread Light switch BRD4304A']  = { this.buildOpenThreadExample("light-switch-app", "BRD4304A")  }

            parallelNodes['Build OpenThread Window BRD4161A']        = { this.buildOpenThreadExample("window-app", "BRD4161A")  }
            parallelNodes['Build OpenThread Window BRD4164A']        = { this.buildOpenThreadExample("window-app", "BRD4164A")  }
            parallelNodes['Build OpenThread Window BRD4166A']        = { this.buildOpenThreadExample("window-app", "BRD4166A")  }
            parallelNodes['Build OpenThread Window BRD4186C']        = { this.buildOpenThreadExample("window-app", "BRD4186C")  }
            parallelNodes['Build OpenThread Window BRD4187C']        = { this.buildOpenThreadExample("window-app", "BRD4187C")  }
            parallelNodes['Build OpenThread Window BRD2601B']        = { this.buildOpenThreadExample("window-app", "BRD2601B")  }
            parallelNodes['Build OpenThread Window BRD2703A']        = { this.buildOpenThreadExample("window-app", "BRD2703A")  }
            // parallelNodes['Build OpenThread Window BRD4304A']        = { this.buildOpenThreadExample("window-app", "BRD4304A")  }
//TODO
        //    parallelNodes['Build Custom Examples BRD4161A']      = { this.buildSilabsCustomOpenThreadExamples("BRD4161A") }
         //   parallelNodes['Build Custom Examples BRD4186C']      = { this.buildSilabsCustomOpenThreadExamples("BRD4186C") }
        }

    
        parallelNodes['Build Wifi Lighting']        = { this.buildWiFiLighting()   }
        parallelNodes['Build Wifi Lock']            = { this.buildWiFiLock()       }
        parallelNodes['Build Chip-tool ']           = { this.buildChipTool()   }

        parallelNodes.failFast = false
        parallel parallelNodes
    }
    // 
    stage("Push to Nexus")
    {
        advanceStageMarker()
        pushToNexus()
    }
     stage('SQA')
    {
      // advanceStageMarker()
        //even openthread test in parallel, they actually run in sequence as they are using same raspi
        def parallelNodes = [:]

        parallelNodes['Junit lighting BRD4161A']       = { this.openThreadTestSuite("lighting","BRD4161A") }
        parallelNodes['Junit lighting BRD4187A']       = { this.openThreadTestSuite("lighting","BRD4187A")   }
                                                                                                                                                                           
        parallelNodes['lighting Thread BRD4187C']   = { this.utfThreadTestSuite('utf_matter_thread','matter_thread','lighting','thread','BRD4187C','',"/manifest-4187-thread","--tmconfig tests/.sequence_manager/test_execution_definitions/matterci_test_sequence_thread.yaml") }     
        parallelNodes['lighting Thread BRD4161A']   = { this.utfThreadTestSuite('utf_matter_thread','matter_thread','lighting','thread','BRD4161A','',"/manifest-4161-thread","--tmconfig tests/.sequence_manager/test_execution_definitions/matterci_test_sequence_thread_4161.yaml") }     
        parallelNodes['lighting Thread BRD2703A']   = { this.utfThreadTestSuite('utf_matter_thread_2','matter_thread_2','lighting','thread','BRD2703A','',"/manifest-2703-thread","--tmconfig tests/.sequence_manager/test_execution_definitions/matterci_test_sequence_thread.yaml") }     
      
        parallelNodes['lighting rs9116 BRD4161A']   = { this.utfWiFiTestSuite('utf_matter_ci','INT0014944','lighting','wifi','BRD4161A','rs9116','',"/manifest","--tmconfig tests/.sequence_manager/test_execution_definitions/matterci_test_sequence.yaml") }          
                                                                         
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
