pipeline {
  // Note: we don't really need this specific runner. This is a workaround for
  // non-existent IT/Automation support. Existing Jenkins runners do not have
  // uniform access to components and IT infrasrtructure.  For example we can
  // easily end up on the machine without access to artifactory.  Since IT
  // support is abysmall/non existed I just switched to a runner that **works**
  agent { label 'zalman' }
  options {
    skipDefaultCheckout(true)
  }
  environment {
    JENKINS_CONTROLLER="http://jenkins.dev.syntacore.com"
    USR="ie-sc"
    PASS="1"
    ZALMAN_NIGHTLY="${JENKINS_CONTROLLER}/job/Tools/job/openocd_nightly_zalman/lastBuild/api/xml"
    TWIN_NIGHTLY="${JENKINS_CONTROLLER}/job/Tools/job/openocd_nightly_twin/lastBuild/api/xml"
    TWIN_NIGHTLY_UNSTABLE="${JENKINS_CONTROLLER}/job/Tools/job/openocd_nightly_twin_unstable/lastBuild/api/xml"
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
    stage('GetTwinNightlyUnstable') {
      steps {
        // Since this is an unstable platform, we just fetch the results
        // but the result file DOES NOT have an "xml" extension, so
        // it won't be processed later
        sh 'curl -u $USR:$PASS ${TWIN_NIGHTLY_UNSTABLE} -o twin_unstable.info'

        // this renders this stage as "FAILED" on a dashboard
        catchError(buildResult: 'SUCCESS', stageResult: 'FAILURE') {
          sh "exit 1"
        }
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
           cc: 'konstantin.vladimirov@syntacore.com,evgeniy.naydanov@syntacore.com',
           to: 'anatoly.parshintsev@syntacore.com'
    }
  }
}
