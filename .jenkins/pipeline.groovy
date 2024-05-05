@Library("jenkins-lib@v3-volatile") _

import tools.automation.TI

def buildProject(vars, target = "openocd") {
  sh(""" ./make.py just-config --profile:host ${vars.profile} \
          --options:host build_type=${vars.buildType} \
          --build-path build/${vars.buildType} ${vars.testOpt} """)
  sh("./make.py build --build-path build/${vars.buildType} --target ${target}")
}

workflow('openocd') {

    parameters {
        [string(name: 'fpgaTestsWorkspaceId', defaultValue: null,
                description: 'Tests on FPGA: Predefined HWRS reversion id for nightly runs'),
         string(name: 'fpgaTestsPlatformTimeoutMinutes', defaultValue: '10',
                description: 'Tests on FPGA: Maximum minutes to run test for individual platform'),
         choiceParam(name: 'fpgaTestsPlatformsList',
                     choices: [
                        'fpga_info/TWIN_NIGHTLY_CONFIGURATIONS.list',
                        'fpga_info/TWIN_UNSTABLE_CONFIGURATIONS.list',
                        'fpga_info/TWIN_SCR9_CONFIGURATIONS.list'
                     ],
                     defaultValue: 'fpga_info/TWIN_NIGHTLY_CONFIGURATIONS.list',
                     description: 'Tests on FPGA: run tests on platforms listed in specified file')]
    }

    job('lint') {
        matrix {
            [["image": ['cpp_ubuntu_20']]]
        }
        script { vars -> makepy.lint() }
    }

    job('main-build') {
        resources {
            cpu('0.8', '4')
            memory('0.3Gi', '16Gi')
        }
        matrix {
            [[image          : ['cpp_ubuntu_18', 'cpp_centos_7', 'cpp_rocky_8', 'cpp_ubuntu_20', 'cpp_ubuntu_22'],
              testOpt        : [''],
              buildType      : ['Release'],
              profile        : ['default']],
             [image          : ['cpp_ubuntu_22'],
              testOpt        : [''],
              buildType      : ['Release'],
              profile        : ['makepy_sc_mingw']]]
        }
        script { vars -> buildProject(vars) }
    }

    job('fpga-postcommit') {
        resources {
            cpu('0.1', '1')
            memory('0.1Gi', '4Gi')
        }
        matrix {
            [[image        : ['cpp_ubuntu_20']]]
        }
        rules { vars ->
            include(vars.ti >= TI.POSTCOMMIT)
        }
        script { vars ->
            withCredentials([string(credentialsId: 'artifactory_cicdsc_api_key', variable: 'ART_API_KEY')]) {
                sh("./.ci/utils/check_nightly_status.sh $ART_API_KEY")
            }
        }
    }

    job('tests-sanitized-nightly') {
        resources {
            cpu('10', '10')
            memory('16Gi')
        }
        dependsOn 'lint' // workaround for better stage scheduling
        matrix {
             [[image          : ['cpp_ubuntu_20'],
               profile        : ['default'],
               testingType    : ['spike'],
               // unfortunatly, we can't enable Strict sanitization, since jimtcl has bugs like this:
               // https://github.com/msteveb/jimtcl/issues/300
               // https://github.com/msteveb/jimtcl/issues/301
               testOpt        : ['--options:host test=True --sanitize-level=Enabled',
                                 '--options:host test=True --tests-options tests-valgrid-path=valgrind'],
               buildType      : ['Debug', 'Release']]]
        }
        rules { vars ->
            include(vars.ti >= TI.POSTCOMMIT)
        }
        script { vars ->
            buildProject(vars)
            try {
              sh("./make.py build --build-path build/${vars.buildType} --target OpenOCDTestsOn_spike --parallel 8")
              sh("./make.py sh ./.makepy/support/utils/check_sanitizer_logs.sh build/${vars.buildType}/testing/dejagnu")
            } catch (Exception ex) {
              artifacts.push("build/${vars.buildType}/testing",
                             "artifacts-${vars.testingType}-${vars.image}-${vars.profile}-${vars.buildType}",
                             retention: '1w')
              error "wasted!"
            }
        }
    }

    job('tests') {
        resources {
            cpu('10', '10')
            memory('0.4Gi', '16Gi')
        }
        dependsOn 'lint' // workaround for better stage scheduling
        matrix {
            [[image          : ['cpp_ubuntu_18', 'cpp_ubuntu_20'],
              profile        : ['default'],
              testingType    : ['spike', 'external'],
              testOpt        : ['--options:host test=True'],
              buildType      : ['Release']]]
        }
        script { vars ->
          buildProject(vars)
          try {
            if (vars.testingType == 'spike') {
              sh("./make.py build --build-path build/${vars.buildType} --target OpenOCDTestsOn_spike --parallel 8")
            } else {
              sh("./make.py build --build-path build/${vars.buildType} --target RISCVTestsDebug --parallel 8")
            }
          } catch (Exception ex) {
            artifacts.push("build/${vars.buildType}/testing",
                           "artifacts-${vars.testingType}-${vars.image}-${vars.profile}-${vars.buildType}",
                           retention: '1w')
            error "wasted!"
          }
        }
    }

    job('tests-transferable') {
        resources {
            cpu('10', '10')
            memory('0.7Gi', '16Gi')
        }
        dependsOn 'lint' // workaround for better stage scheduling
        matrix {
            [[image          : ['cpp_ubuntu_20'],
              profile        : ['default'],
              testOpt        : ['--options:host test=True'],
              buildType      : ['Release']]]
        }
        script { vars ->
            buildProject(vars)
            try {
                sh("./make.py build --build-path build/${vars.buildType} --target transferable_testsuite")
                sh("./make.py build --build-path build/${vars.buildType} --target Transferable_OpenOCDTestsOn_spike --parallel 8")
            } catch (Exception ex) {
                artifacts.push("build/${vars.buildType}/testing",
                    "artifacts-transferable--${vars.image}-${vars.profile}-${vars.buildType}",
                    retention: '1w')
                error "wasted!"
            }
        }
    }

    job('tests-fpga-build') {
        resources {
            cpu('4', '4')
            memory('16Gi')
        }
        matrix {
            [[image          : ['cpp_ubuntu_20'],
              profile        : ['default'],
              testOpt        : ['--options:host test=True'],
              buildType      : ['Release']]]
        }
        rules { vars ->
            include(vars.ti >= TI.POSTCOMMIT)
        }
        script { vars ->
            buildProject(vars, "transferable_testsuite")
            artifacts.push("build/${vars.buildType}/install_testsuite", "${vars.profile}/${vars.image}/testsuite'")

            sh("./make.py sh make fpga_configuration_registry -f testing/syntacore/fpga_support/makefile")
            artifacts.push("fpga_info", "${vars.profile}/${vars.image}/fpga_info'")
        }
    }

    job('tests-fpga-run') {
        resources {
            cpu('0.5', '1')
            memory('4Gi')
        }
        dependsOn 'tests-fpga-build'
        matrix {
            [[image          : ['cpp_ubuntu_20'],
              profile        : ['default'],
              buildType      : ['Release'],
              stand          : ['twin']]]
        }
        rules { vars ->
            include(vars.ti >= TI.POSTCOMMIT)
        }
        script { vars ->
            def stand = "fpga_${vars.stand}"
            def host = "${vars.stand}.lab.dev.syntacore.com"
            def workspaceId = params.fpgaTestsWorkspaceId ?: env.BUILD_TAG
            def maxDurationMinutesPerConfiguration = params.fpgaTestsPlatformTimeoutMinutes as Integer

            artifacts.pop("${vars.profile}/${vars.image}/testsuite'")
            artifacts.pop("${vars.profile}/${vars.image}/fpga_info'")

            def configurations = readFile(params.fpgaTestsPlatformsList).tokenize("\n")
            def maxDurationMinutes = maxDurationMinutesPerConfiguration * configurations.size()

            sh(""" ./make.py dev lock --name ${stand} \
                    --duration ${maxDurationMinutes}m \
                    --override-workspace-id ${workspaceId} \
                    --blocking """)

            def options = """ --build-path build/${vars.buildType} \
                                      --host ${host} \
                                      --override-workspace-id ${workspaceId} """

            try {
                sh("./make.py test-suite --install ${options}")

                for (configuration in configurations) {
                    // we need to run tests for all configurations, so should continue on failure
                    catchError(buildResult: 'FAILURE', stageResult: 'FAILURE') {
                        try {
                            timeout(maxDurationMinutesPerConfiguration) {
                                sh("./make.py test-suite --run ${configuration} ${options}")
                            }
                        } catch (Throwable e) {
                            artifacts.push(
                                    "build/${vars.buildType}/testing/lgrw/${configuration}.tar.gz",
                                    "logs",
                                    retention: '1w'
                            )
                            throw e
                        }
                    }
                }
            } finally {
                sh("./make.py test-suite --cleanup ${options} || true")
                sh("./make.py dev unlock --override-workspace-id ${workspaceId}")
            }
        }
    }

    deploy {
        resources {
            cpu('1.2', '4')
            memory('0.3Gi', '16Gi')
        }
        matrix {
            [[image          : ['cpp_ubuntu_18', 'cpp_centos_7', 'cpp_rocky_8', 'cpp_ubuntu_20', 'cpp_ubuntu_22'],
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
            [[image          : ['cpp_ubuntu_18', 'cpp_centos_7', 'cpp_rocky_8', 'cpp_ubuntu_20', 'cpp_ubuntu_22'],
              profile        : ['default']],
             [image          : ['cpp_ubuntu_22'],
              profile        : ['makepy_sc_mingw']]]
        }
        rules { vars ->
            include(vars.name in vars.deploy)
        }
        script { vars ->
            String packageRef = makepy.getConanVars(vars.name, vars.assumeRelease as Boolean).packageRef
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
