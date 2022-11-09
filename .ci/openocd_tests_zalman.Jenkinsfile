pipeline {
  agent { label 'zalman' }
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
    stage('Build') {
      steps {
        echo "Building project"
        dir ("$WD") {
          sh 'printenv'
          sh 'make in_docker TARGET=build -f ${WD}/.ci/makefile'
        }
      }
    }
    stage('Test') {
      matrix {
        axes {
          axis {
            name 'BOARD'
            values 'arty100_scr1_32',
                   'arty100_scr3_32',
                   'arty100_scr4_32_imcaf',
                   'arty100_scr4_32_imcafd'
          }
        }
        stages {
          stage('RunTests') {
            steps {
              lock (resource: "lock-fpga-on-zalman") {
                dir ("$WD") {
                  sh 'make prepare_board -f ${WD}/.ci/makefile TARGET_BOARD=${BOARD}'
                  sh 'make in_docker -f ${WD}/.ci/makefile TARGET=test TARGET_BOARD=${BOARD}'
                }
              }
            }
          }
        }
      }
    }
  }
  post {
    always {
      sh 'make clean_docker -f ${WD}/.ci/makefile'
    }
  }
}
