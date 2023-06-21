def boards = []
def UploadResults = 0

def runTests(boards){
  tests = [:]
  for (brbd in boards) {
    // https://devops.stackexchange.com/a/8875
    def board = brbd
    tests["${board}"] = {
      stage("${board}") {
         lock (resource: "${fpga_lock}") {
            dir ("$WD") {
              sh """
              #!/bin/bash
              set +x
              echo "${board}"
              ${MAKE_PY} sh \
                make prepare_board \
                    -f ${SOURCE_DIR}/testing/syntacore/fpga_support/makefile \
                    TARGET_BOARD=${board}
              ${MAKE_PY} --image ${DOCKER_IMAGE} -l debug build \
                    -b ${BUILD_DIR} --target OpenOCDTestsOn_${board}
              """
            }
         }
      }
    }
  }
  parallel tests
}

pipeline {
  agent { label params.AGENT }
  options {
    skipDefaultCheckout(true)
    timestamps()
  }
  environment {
    BUILD_ID = "${BUILD_TAG}"
    STAND_ID = "${params.AGENT}"

    WD = "${WORKSPACE}/${BUILD_TAG}"
    SOURCE_DIR = "${WD}/openocd_sources"
    BUILD_CONFIG = "Release"
    BUILD_MOUNT = "${WD}/build"
    BUILD_DIR = "${BUILD_MOUNT}/${BUILD_CONFIG}"

    MAKE_PY = "${SOURCE_DIR}/make.py"

    PYTHON_DIR = "${WD}/python"
    PYTHON_INSTALL = "$PYTHON_DIR/install"
    PYTHON_BIN_DIR = "$PYTHON_INSTALL/bin"
    PATH = "$PYTHON_BIN_DIR:${env.PATH}"

    // in addition NAS_PSW and NAS_USR variables are defined
    NAS = credentials('GitlabJenkins')
    DOCKER = credentials('docker-images-nexus')
    DOCKER_IMAGE = "cpp_ubuntu_18"

    SUDO_PSW = "${NAS_PSW}"
    ARTIFACTORY_API_KEY = credentials('OpenOCDTestReportKey')
  }
  stages {
    stage('CleanWorkspaceAndCheckout') {
      steps {
        cleanWs()
        dir ("$SOURCE_DIR") {
          checkout scm
        }
      }
    }
    stage('PythonBuild') {
      steps {
        dir("$PYTHON_DIR") {
          script {
            sh """
            #!/bin/bash
            wget 'https://www.python.org/ftp/python/3.10.0/Python-3.10.0.tar.xz'
            tar -xvf Python-3.10.0.tar.xz
            cd Python-3.10.0
            ./configure --prefix=$PYTHON_INSTALL
            make install -j8
            """
          }
        }
      }
    }
    stage('PrepareCredentials') {
      steps {
        echo "Generating credential file"
        dir ("$WD") {
          script {
            withCredentials([file(credentialsId: 'makepy_creds', variable: 'MAKEPY_CREDS')]) {
            withCredentials([sshUserPrivateKey(credentialsId: 'cicd-sc_gitlab_ssh_key', keyFileVariable: 'MAKEPY_SSH')]) {
              sh """
              #!/bin/bash
              set +x
              wget https://github.com/stedolan/jq/releases/download/jq-1.6/jq-linux64 -O jq && chmod +x jq
              cp "$MAKEPY_SSH" the_key
              cat "$MAKEPY_CREDS"  | ./jq ".gitlab.ssh_path = \\\"$WD/the_key\\\"" | tee credentials.json
              export GIT_SSH_COMMAND="ssh -o StrictHostKeyChecking=no -i '$MAKEPY_SSH'"
              ${MAKE_PY} pass
              """
            }
            }
          }
        }
      }
    }
    stage('Build') {
      steps {
        echo "Building project"
        dir ("$BUILD_MOUNT") {
          sh '${MAKE_PY} --image $DOCKER_IMAGE container run -p -m . --credentials ${WD}/credentials.json'
          sh '${MAKE_PY} --image $DOCKER_IMAGE sh --container-user root apt-get install telnet'
          sh '${MAKE_PY} --image $DOCKER_IMAGE conan-config --credentials ${WD}/credentials.json'
          sh '${MAKE_PY} --image $DOCKER_IMAGE just-config -b ${BUILD_DIR} --profile:host default'
          sh '${MAKE_PY} --image $DOCKER_IMAGE build -b ${BUILD_DIR} --target openocd'
        }
      }
    }
    stage('PrepareBoards') {
      steps {
        dir ("$WD") {
          // fpga_configuration_registry creates **fpga_info** directory
          sh "${MAKE_PY} sh make fpga_configuration_registry -f ${SOURCE_DIR}/testing/syntacore/fpga_support/makefile"
          script {
            platform_list = "NO_PLATFROM_LIST_SELECTED"
            switch(params.AGENT) {
            case 'twin_server':
              env.fpga_lock = 'lock-fpga-on-twin'
              if (params.containsKey('use_unstable_platforms')) {
                platform_list = 'fpga_info/TWIN_UNSTABLE_CONFIGURATIONS.list'
              } else if (params.containsKey('scr9_validation')) {
                platform_list = 'fpga_info/TWIN_SCR9_CONFIGURATIONS.list'
              } else {
                platform_list = 'fpga_info/TWIN_NIGHTLY_CONFIGURATIONS.list'
                UploadResults = 1
              }
              break
            case 'zalman':
              env.fpga_lock = 'lock-fpga-on-zalman'
              platform_list = 'fpga_info/ZALMAN_NIGHTLY_CONFIGURATIONS.list'
              UploadResults = 1
              break
            default:
              currentBuild.result = 'ABORTED'
              error "What the hell I'm doing here? I don't belong to ${params.AGENT}"
              break
            }
            echo "selected platform list: ${platform_list}"
            platforms = readFile("${platform_list}")
            echo "${platforms}"
            boards = platforms.tokenize("\n")
          }
        }
        sh 'printenv'
      }
    }
    stage('RunTests') {
      steps {
        script {
          runTests(boards)
        }
      }
    }
  }
  post {
    always {
      sh "${MAKE_PY} history"
      sh "${MAKE_PY} container clean"
      sh "${SOURCE_DIR}/.ci/utils/upload_testing_results.sh ${BUILD_DIR}/testing ${BUILD_ID} ${ARTIFACTORY_API_KEY}"
    }
    success {
      sh "${SOURCE_DIR}/.ci/utils/report_test_success.sh ${UploadResults} ${STAND_ID} ${ARTIFACTORY_API_KEY}"
    }
  }
}
