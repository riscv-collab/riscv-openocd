pipeline {
  agent { label "spb-win11" }
  options {
    skipDefaultCheckout(true)
    timestamps()
  }
  environment {
    WD = "${WORKSPACE}/${BUILD_TAG}"
    DISTRIBUTION_ID = "${params.DISTR_ID}"
  }
  stages {
    stage('CleanWorkspaceAndCheckout') {
      steps {
        cleanWs()
        dir ("$WD") {
          checkout scm
        }
      }
    }
    stage('RunTest') {
      steps {
        dir ("$WD") {
          powershell(script: '.ci/utils/windows_run.ps1')
        }
      }
    }
  }
}


