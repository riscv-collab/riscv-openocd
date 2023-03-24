def boards = []

def runTests(boards){
  tests = [:]
  for (brbd in boards) {
    // https://devops.stackexchange.com/a/8875
    def board = brbd
    tests["${board}"] = {
      stage("${board}") {
         lock (resource: "${fpga_lock}") {
            dir ("$WD") {
              echo "${board}"
              sh "make prepare_board -f ${WD}/.ci/makefile TARGET_BOARD=${board}"
              sh "make in_docker -f ${WD}/.ci/makefile TARGET=test TARGET_BOARD=${board}"
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
    WD = "${WORKSPACE}/${BUILD_TAG}"
    // in addition NAS_PSW and NAS_USR variables are defined
    NAS = credentials('GitlabJenkins')
    DOCKER = credentials('docker-images-nexus')
    SUDO_PSW = "${NAS_PSW}"
    // needed by docker CI scipts
    COMMON_BUILD_DIR = "$WD/build"
    DOCKER_CONTAINER_NAME = "OpenOCD_CI_CONTAINER"

    ARTIFACTORY_API_KEY = credentials('OpenOCDTestReportKey')
    BUILD_ID = "${BUILD_TAG}"
    STAND_ID = "${params.AGENT}"
  }
  stages {
    stage('CleanWorkspaceAndCheckout') {
      steps {
        sh "docker login -u ${DOCKER_USR} -p ${DOCKER_PSW} nexus.dev.syntacore.com:8091"
        cleanWs()
        dir ("$WD") {
          checkout scm
        }
      }
    }
    stage('SetupEnvironment') {
      steps {
        script {
          switch(params.AGENT) {
          case 'twin_server':
            env.fpga_lock = 'lock-fpga-on-twin'
            boards = [
              'twin_scr5_32',
              'twin_scr5_64',
              'twin_scr6',
              'twin_scr7evalcluster_pseudoscore_nortos',
              'twin_scr7evalcluster_pseudoscore_nortos_workarea',
              'twin_scr7evalcluster_mcore4_nortos',
              'twin_scr7evalcluster_mcore4_nortos_workarea',
              'twin_scr7evalcluster_smp4_workarea',
              'twin_scr7evalcluster_smp4'
            ]

            if (params.containsKey('use_unstable_platforms')) {
              boards = [
                'twin_scr7RVV_mcore2_nortos',
                'twin_scr7bug21107_score_nortos',
                'twin_SCR7dev_mcore2_nortos',
                'twin_SCR7dev_mcore2_rtoshw',
                'twin_SCR9dev_score_nortos',
                'twin_SCR9dev_score_rtoshw'
              ]
            }

            if (params.containsKey('scr9_validation')) {
              boards = [
                'twin_norvv_SCR9dev_score_nortos',
                'twin_norvv_SCR9dev_score_rtoshw',
                'twin_norvv_scr9hpfpu200323_score_rtoshw'
              ]
            }

            break
          case 'zalman':
            env.fpga_lock = 'lock-fpga-on-zalman'
            boards = ['arty100_scr1_32',
                      'arty100_scr3_32',
                      'arty100_scr4_32_imcaf',
                      'arty100_scr4_32_imcafd']
            break
          default:
            currentBuild.result = 'ABORTED'
            error "What the hell I'm doing here? I don't belong to ${params.AGENT}"
            break
          }
        }
        sh 'printenv'
      }
    }
    stage('Build') {
      steps {
        echo "Building project"
        dir ("$WD") {
          sh 'make in_docker TARGET=build -f ${WD}/.ci/makefile'
        }
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
      sh 'make in_docker TARGET=upload_test_report -f ${WD}/.ci/makefile'
    }
    success {
      sh 'make in_docker TARGET=record_test_success -f ${WD}/.ci/makefile'
    }
    cleanup {
      sh 'make clean_docker -f ${WD}/.ci/makefile'
    }
  }
}
