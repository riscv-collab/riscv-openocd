pipeline {
  agent { label 'msk_automation_linux_01' }
  options {
    skipDefaultCheckout(true)
  }
  environment {
    JENKINS_CONTROLLER="http://jenkins.dev.syntacore.com"
    USR="ie-sc"
    PASS="1"
    ZALMAN_NIGHTLY="${JENKINS_CONTROLLER}/job/Tools/job/openocd_nightly_twin/lastBuild/api/xml"
    TWIN_NIGHTLY="${JENKINS_CONTROLLER}/job/Tools/job/openocd_nightly_zalman/lastBuild/api/xml"
  }
  stages {
    stage('CleanWorkspaceAndCheckout') {
      steps {
        cleanWs()
        checkout scm
      }
    }
    stage('GetZalmanNightly') {
      steps {
        sh 'curl -u $USR:$PASS ${ZALMAN_NIGHTLY} -o zalman.info.xml'
      }
    }
    stage('GetTwinNightly') {
      steps {
        sh 'curl -u $USR:$PASS ${TWIN_NIGHTLY} -o twin.info.xml'
      }
    }

    stage('ProcessResults') {
      steps {
        sh './.ci/utils/process_job_info.sh'
      }
    }
  }
  post {
    failure {
      script {
        MSG="<b>nightly failure</b>\
             <br>Project: ${env.JOB_NAME} \
             <br>Build URL: <a href='${env.BUILD_URL}'>Build #${env.BUILD_NUMBER}</a>"
      }
      mail body: "${MSG}",
           charset: 'UTF-8',
           mimeType: 'text/html',
           subject: 'OpenOCD nightly failure',
           to: 'anatoly.parshintsev@syntacore.com'
    }
  }
}
