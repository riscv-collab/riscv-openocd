@Library('jenkins-lib@v4.2.x') _

import tools.automation.TI

def buildProject(vars, target = "openocd") {
  sh(""" ./make.py just-config --profile:host ${vars.profile} \
          --settings:host "&:build_type=${vars.buildType}" \
          --build-path build/${vars.buildType} ${vars.extraOpts} """)
  sh("./make.py build --build-path build/${vars.buildType} --target ${target}")
}

workflow('openocd') {

    parameters {
        [string(name: 'fpgaTestsWorkspaceId', defaultValue: null,
                description: 'Tests on FPGA: Predefined HWRS reversion id for nightly runs'),
         string(name: 'fpgaTestsReservationMaxWaitHours', defaultValue: '48',
                description: 'Tests on FPGA: Maximum time to wait for a reservation'),
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
        resources {
            cpu('0.2', '1.9')
            memory('0.1Gi', '0.8Gi')
            fs('1.0Gi', '1.0Gi')
        }
        matrix {
            [["image": ['cpp_ubuntu_20']]]
        }
        shellScript {
            '''
                sc-jenkins-lib lint
            '''
        }
    }

    job('main-build') {
        resources {
            cpu('0.8', '4')
            memory('0.3Gi', '16.2Gi')
            fs('1.0Gi', '1.2Gi')
        }
        matrix {
            [[image          : ['cpp_centos_7', 'cpp_rocky_8', 'cpp_ubuntu_18', 'cpp_ubuntu_20', 'cpp_ubuntu_22'],
              extraOpts      : ['', '--options:host elct_support=True'],
              buildType      : ['Release'],
              profile        : ['default']],
             [image          : ['cpp_ubuntu_22'],
              extraOpts      : [''],
              buildType      : ['Release'],
              extraOpts      : ['', '--options:host elct_support=True'],
              profile        : ['makepy_sc_mingw', 'mp_armhf']]]
        }
        script { vars -> buildProject(vars) }
    }

    job('tests-sanitized') {
        resources {
            cpu('6.1', '10')
            memory('1.1Gi', '17.1Gi')
            fs('12.5Gi', '22.6Gi')
        }
        dependsOn 'lint' // workaround for better stage scheduling
        matrix {
             [[image          : ['cpp_ubuntu_20'],
               profile        : ['default'],
               testingType    : ['spike'],
               // unfortunatly, we can't enable Strict sanitization, since jimtcl has bugs like this:
               // https://github.com/msteveb/jimtcl/issues/300
               // https://github.com/msteveb/jimtcl/issues/301
               extraOpts      : ['--options:host test=True --sanitize-level=Enabled',
                                 '--options:host test=True --sanitize-level=Enabled --options:host elct_support=True',
                                 '--options:host test=True --tests-options tests-valgrid-path=valgrind',
                                 '--options:host test=True --tests-options tests-valgrid-path=valgrind --options:host elct_support=True'],
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
            cpu('10.0', '10')
            memory('0.6Gi', '23.5Gi')
            fs('13.0Gi', '30.1Gi')
        }
        dependsOn 'lint' // workaround for better stage scheduling
        matrix {
            [[image          : ['cpp_rocky_8', 'cpp_ubuntu_20'],
              profile        : ['default'],
              testingType    : ['spike', 'external'],
              extraOpts      : ['--options:host test=True',
                                '--options:host elct_support=True --options:host test=True'],
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
            cpu('10.0', '10')
            memory('0.8Gi', '22.8Gi')
            fs('15.0Gi', '33.3Gi')
        }
        dependsOn 'lint' // workaround for better stage scheduling
        matrix {
            [[image          : ['cpp_ubuntu_20'],
              profile        : ['default'],
              extraOpts      : ['--options:host test=True',
                                '--options:host elct_support=True --options:host test=True'],
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
            cpu('0.9', '4')
            memory('0.5Gi', '16Gi')
            fs('5.8Gi', '13.5Gi')
        }
        matrix {
            [[image          : ['cpp_ubuntu_20'],
              profile        : ['default'],
              extraOpts      : ['--options:host elct_support=True --options:host test=True'],
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

    job('tests-fpga-run-nightly') {
        timeout(8 + (params.fpgaTestsReservationMaxWaitHours as Integer))
        resources {
            cpu('0.1', '1')
            memory('0.3Gi', '4Gi')
            fs('2.9Gi', '5.8Gi')
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
            def reservationMaxWaitHours = params.fpgaTestsReservationMaxWaitHours as Integer

            artifacts.pop("${vars.profile}/${vars.image}/testsuite'")
            artifacts.pop("${vars.profile}/${vars.image}/fpga_info'")

            def configurations = readFile(params.fpgaTestsPlatformsList).tokenize("\n")
            def maxDurationMinutes = maxDurationMinutesPerConfiguration * configurations.size()

            def options = """ --build-path build/${vars.buildType} \
                                      --host ${host} \
                                      --override-workspace-id ${workspaceId} """

            try {
                sh(""" ./make.py dev lock --name ${stand} \
                                          --duration ${maxDurationMinutes}m \
                                          --max-wait ${reservationMaxWaitHours}h \
                                          --override-workspace-id ${workspaceId} \
                                          --blocking """)
                timeout(maxDurationMinutes) {
                    try {
                        sh("./make.py test-suite --install --force ${options}")

                        for (configuration in configurations) {
                            // we need to run tests for all configurations, so should continue on failure
                            catchError(buildResult: 'FAILURE', stageResult: 'FAILURE') {
                                try {
                                    sh(""" ./make.py test-suite --run-platform ${configuration} \
                                            --run-tool jtag ocd utils \
                                            ${options} """)
                                } finally {
                                    artifacts.push(
                                            "build/${vars.buildType}/testing/lgrw/${configuration}.tar.gz",
                                            "logs",
                                            retention: '1w')
                                }
                            }
                        }
                    } finally {
                        sh("./make.py test-suite --cleanup ${options}")
                    }
                }
            } finally {
                sh("./make.py dev unlock --override-workspace-id ${workspaceId}")
            }
        }
    }

    job('tests-on-dummy') {
        resources {
            cpu('0.9', '4')
            memory('0.1Gi', '0.7Gi')
            fs('1.0Gi', '1.0Gi')
        }
        matrix {
            [[image          : ['cpp_ubuntu_20'],
              env            : ['', 'CC=clang CFLAGS=-fsanitize=address,undefined LDFLAGS=-Wl,-ldl']]]
        }
        script {
            sh('''
                BUILD_DIR="build/tests_on_dummy_Build"
                mpy sh env ${VARS_env} .makepy/support/utils/run_tests_on_dummy.sh 4 ${BUILD_DIR}
            ''')
        }
    }

    deploy {
        resources {
            cpu('0.8', '4')
            memory('0.4Gi', '16Gi')
            fs('1.0Gi', '1.5Gi')
        }
        matrix {
            [[image          : ['cpp_centos_7', 'cpp_rocky_8', 'cpp_ubuntu_18', 'cpp_ubuntu_20', 'cpp_ubuntu_22'],
              profile        : ['default'],
              extraArgs      : ['', '--options:host elct_support=True']],
             [image          : ['cpp_ubuntu_22'],
              profile        : ['makepy_sc_mingw'],
              extraArgs      : ['', '--options:host elct_support=True']],
             [image          : ['cpp_ubuntu_22'],
              profile        : ['mp_armhf'],
              extraArgs      : ['--options:host elct_support=True']]]
        }
    }

    job('deploy_artifactory') {
        resources {
            cpu('0.2', '4')
            memory('0.1Gi', '8Gi')
            fs('1.0Gi', '1.0Gi')
        }
        dependsOn 'deploy'
        matrix {
            [[image          : ['cpp_centos_7', 'cpp_rocky_8', 'cpp_ubuntu_18', 'cpp_ubuntu_20', 'cpp_ubuntu_22'],
              profile        : ['default']],
             [image          : ['cpp_ubuntu_22'],
              profile        : ['makepy_sc_mingw', 'mp_armhf']]]
        }
        rules { vars ->
            include(vars.name in vars.deploy)
        }
        shellScript {
            '''
                PACKAGE_REF=$(OPTS=; [[ "${VARS_assumeRelease}" ]] && OPTS="${OPTS} --assume-release"; sc-jenkins-lib get-conan-var  "${VARS_name}" package ${OPTS})
                 mpy conan install --profile:host ${VARS_profile} --remote syntacore --requires "${PACKAGE_REF}" --lockfile-partial --output-folder build/deploy --deployer .makepy/support/utils/conan_the_deployer.py 
                 mpy sh .makepy/support/utils/upload_development_build.sh $(find build/deploy -maxdepth 1 -name "*bundle*") ${ART_API_KEY}
            '''
        }
    }
}
