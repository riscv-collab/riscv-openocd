pipeline {
  agent { label 'zalman' }
  environment {
    WD = "${WORKSPACE}/${BUILD_TAG}"
    // in addition NAS_PSW and NAS_USR variables are defined
    NAS = credentials('GitlabJenkins')
    SUDO_PSW = "${NAS_PSW}"
    // needed by docker CI scipts
    COMMON_BUILD_DIR = "$WD/build"
  }
  stages {
    stage('Build') {
      steps {
        echo "Building project"
        dir ("$WD") {
          sh 'printenv'
          sh 'make in_docker TARGET=build -f ${WORKSPACE}/.ci/makefile'
        }
      }
    }
    stage('Test') {
      matrix {
        axes {
          axis {
            name 'BOARD'
            values 'arty100_scr1_32', 'arty100_scr3_32', 'arty100_scr4_32'
          }
        }
        stages {
          stage('LockBoard') {
            options { lock( 'lock-board' ) }
            stages {
              stage('BoardPrepare') {
                steps {
                  dir ("$WORKSPACE") {
                    sh 'make prepare_board -f ${WORKSPACE}/.ci/makefile TARGET_BOARD=${BOARD}'
                  }
                }
              }
              stage('BoardTest') {
                steps {
                  dir ("$WD") {
                    sh 'make in_docker -f ${WORKSPACE}/.ci/makefile TARGET=test TARGET_BOARD=${BOARD}'
                  }
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
      sh 'make clean_docker -f ${WORKSPACE}/.ci/makefile'
    }
  }
}
