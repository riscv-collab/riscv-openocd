@Library("jenkins-lib@v3-volatile") _

import tools.automation.TI

def buildProject(vars) {
  sh("./make.py just-config --profile:host ${vars.profile} --build-path ${vars.buildPath} ${vars.testOpt}")
  sh("./make.py build --build-path ${vars.buildPath} --target openocd")
}

workflow('openocd') {
    parameters {
        [
          stringParam(name: 'releaseString', defaultValue: '', description: ''),
        ]
    }

    job('lint') {
        matrix {
            [["image": ['cpp_ubuntu_18']]]
        }
        script { vars -> makepy.lint() }
    }

    job('main-build') {
        resources {
            cpu('4', '4')
            memory('16Gi')
        }
        matrix {
            [[image          : ['cpp_ubuntu_18', 'cpp_centos_7', 'cpp_ubuntu_20', 'cpp_ubuntu_22'],
              testOpt        : [''],
              buildPath      : ['build/Release'],
              profile        : ['default']],
             [image          : ['cpp_ubuntu_22'],
              testOpt        : [''],
              buildPath      : ['build/Release'],
              profile        : ['makepy_sc_mingw']]]
        }
        script { vars -> buildProject(vars) }
    }

    job('tests') {
        resources {
            cpu('10', '10')
            memory('16Gi')
        }
        matrix {
            [[image          : ['cpp_ubuntu_18', 'cpp_ubuntu_20'],
              profile        : ['default'],
              testingType    : ['spike', 'external'],
              testOpt        : ['--options:host test=True'],
              buildPath      : ['build/Release']]]
        }
        script { vars ->
          if (vars.ti >= TI.POSTCOMMIT) {
            withCredentials([string(credentialsId: 'artifactory_cicdsc_api_key', variable: 'ART_API_KEY')]) {
                sh("./.ci/utils/check_nightly_status.sh $ART_API_KEY")
            }
          }

          buildProject(vars)
          try {
            if (vars.testingType == 'spike') {
              sh("./make.py build --build-path ${vars.buildPath} --target OpenOCDTestsOn_spike --parallel 8")
            } else {
              sh("./make.py build --build-path ${vars.buildPath} --target RISCVTestsDebug --parallel 8")
            }
          } catch (Exception ex) {
            artifacts.push("build/Release/testing", "artifacts-${vars.testingType}-${vars.image}-${vars.profile}", retention: 'ignore')
            error "wasted!"
          }
        }
    }

    deploy {
        resources {
            cpu('4', '4')
            memory('16Gi')
        }
        matrix {
            [[image          : ['cpp_ubuntu_18', 'cpp_centos_7', 'cpp_ubuntu_20', 'cpp_ubuntu_22'],
              profile        : ['default']],
             [image          : ['cpp_ubuntu_22'],
              profile        : ['makepy_sc_mingw']]]
        }
    }

    job('deploy_artifactory') {
        resources {
            cpu('4', '4')
            memory('8Gi')
        }
        dependsOn 'deploy'
        matrix {
            [[image          : ['cpp_ubuntu_18', 'cpp_centos_7', 'cpp_ubuntu_20', 'cpp_ubuntu_22'],
              profile        : ['default']],
             [image          : ['cpp_ubuntu_22'],
              profile        : ['makepy_sc_mingw']]]
        }
        rules { vars ->
            include(vars.name in vars.deploy && vars.branch == vars.defaultBranch)
        }
        script { vars ->
            String packageRef = makepy.getConanVars(vars.name, false).packageRef
            sh(""" ./make.py conan install \
                    --profile:host ${vars.profile} --remote syntacore --requires "${packageRef}" --lockfile-partial \
                    --output-folder build/deploy --deployer .makepy/support/utils/conan_the_deployer.py """)
            withCredentials([string(credentialsId: 'artifactory_cicdsc_api_key', variable: 'ART_API_KEY')]) {
                sh(""" ./make.py sh .makepy/support/utils/upload_development_build.sh \
                    \$(find build/deploy -maxdepth 1 -name '*bundle*') $ART_API_KEY """)
            }
        }
    }
}
