@Library('jenkins-lib@v4.2.x') _

import tools.automation.TI

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
            [
                [
                    "image": ['cpp_ubuntu_22']
                ]
            ]
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
            [
                [
                    image    : ['cpp_centos_7', 'cpp_rocky_8', 'cpp_ubuntu_18', 'cpp_ubuntu_20', 'cpp_ubuntu_22'],
                    profile  : ['default'],
                    o_source : ['internal', 'syntacore']
                ], [
                    image    : ['cpp_ubuntu_22'],
                    profile  : ['makepy_sc_mingw', 'mp_armhf'],
                    o_source : ['internal', 'syntacore']
                ]
            ]
        }
        shellScript {
            '''
                source .jenkins/common.sh
                mpy conan create ${VARS_as_conan_args[@]}
            '''
        }
    }

    job('tests-sanitized') {
        resources {
            cpu('6.1', '10')
            memory('1.1Gi', '17.1Gi')
            fs('12.5Gi', '22.6Gi')
        }
        dependsOn 'lint' // workaround for better stage scheduling
            matrix {
                [
                    [
                        image        : ['cpp_ubuntu_20', 'cpp_ubuntu_22'],
                        profile      : ['default'],
                        target       : ['riscv_tests', 'OpenOCDTestsOn_spike'],
                        o_source     : ['internal', 'syntacore'],
                        s_build_type : ['Debug', 'Release'],
                        // TODO: Enable 'strict' sanitize level YCAT-43092
                        o_sanitize   : ['enable'],
                    ]
                ]
            }
        rules { vars ->
            include(vars.ti >= TI.POSTCOMMIT)
        }
        shellScript {
            '''
                source .jenkins/common.sh
                workdir=build/testsuite

                runTests $workdir
                EXIT_CODE=$?

                sc-jenkins-lib artifacts push \
                    "artifacts-${VARS_job}-${VARS_image}-${VARS_profile}-${VARS_target}-${VARS_o_source}-${VARS_s_build_type}-${VARS_o_sanitize}" \
                    "${workdir}/testing" \
                    1w
                exit $EXIT_CODE
            '''
        }
    }

    job('tests-valgrind') {
        resources {
            cpu('10.0', '10')
            memory('0.6Gi', '23.5Gi')
            fs('13.0Gi', '30.1Gi')
        }
        dependsOn 'lint' // workaround for better stage scheduling
        matrix {
            [
                [
                    image        : ['cpp_ubuntu_20', 'cpp_ubuntu_22'],
                    profile      : ['default'],
                    target       : ['riscv_tests', 'OpenOCDTestsOn_spike'],
                    o_source     : ['internal', 'syntacore'],
                    s_build_type : ['Debug', 'Release'],
                ]
            ]
        }
        rules { vars ->
            include(vars.ti >= TI.POSTCOMMIT)
        }
        shellScript {
            '''
                source .jenkins/common.sh
                workdir=build/testsuite

                VARS_tests_valgrind_path=$(which valgrind)
                runTests $workdir
                EXIT_CODE=$?

                sc-jenkins-lib artifacts push \
                    "artifacts-${VARS_job}-${VARS_image}-${VARS_profile}-${VARS_target}-${VARS_o_source}" \
                    "${workdir}/testing" \
                    1w
                exit $EXIT_CODE
            '''
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
            [
                [
                    image    : ['cpp_rocky_8', 'cpp_ubuntu_20', 'cpp_ubuntu_22'],
                    profile  : ['default'],
                    target   : ['riscv_tests', 'OpenOCDTestsOn_spike'],
                    o_source : ['internal', 'syntacore'],
                ]
            ]
        }
        shellScript {
            '''
                source .jenkins/common.sh
                workdir=build/testsuite

                runTests $workdir
                EXIT_CODE=$?

                sc-jenkins-lib artifacts push \
                    "artifacts-${VARS_job}-${VARS_image}-${VARS_profile}-${VARS_target}-${VARS_o_source}" \
                    "${workdir}/testing" \
                    1w
                exit $EXIT_CODE
            '''
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
            [
                [
                    image    : ['cpp_ubuntu_20', 'cpp_ubuntu_22'],
                    profile  : ['default'],
                    target   : ['riscv_tests', 'OpenOCDTestsOn_spike'],
                    o_source : ['internal', 'syntacore'],
                ]
            ]
        }
        shellScript {
            '''
                source .jenkins/common.sh
                workdir=build/testsuite

                runTransferableTests $workdir
                EXIT_CODE=$?

                sc-jenkins-lib artifacts push \
                    "artifacts-${VARS_job}-${VARS_image}-${VARS_profile}-${VARS_target}-${VARS_o_source}" \
                    "${workdir}/testing" \
                    1w
                exit $EXIT_CODE
            '''
        }
    }

    job('tests-fpga-build') {
        resources {
            cpu('0.9', '4')
            memory('0.5Gi', '16Gi')
            fs('5.8Gi', '13.5Gi')
        }
        matrix {
            [
                [
                    image    : ['cpp_ubuntu_22'],
                    profile  : ['default'],
                    o_source : ['internal'],
                ]
            ]
        }
        rules { vars ->
            include(vars.ti >= TI.POSTCOMMIT)
        }
        shellScript {
            '''
                source .jenkins/common.sh
                workdir=build/testsuite

                buildTransferableTestsuite $workdir
                sc-jenkins-lib artifacts push \
                    "${VARS_profile}/${VARS_image}/testsuite" \
                    "${workdir}/install_testsuite"

                mpy sh make fpga_configuration_registry -f testing/syntacore/fpga_support/makefile
                sc-jenkins-lib artifacts push \
                    "${VARS_profile}/${VARS_image}/testsuite" \
                    fpga_info
            '''
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
            [
                [
                    image   : ['cpp_ubuntu_22'],
                    profile : ['default'],
                    stand   : ['twin']
                ]
            ]
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
            [
                [
                    image : ['cpp_ubuntu_20', 'cpp_ubuntu_22'],
                    env   : ['', 'CC=clang CFLAGS=-fsanitize=address,undefined LDFLAGS=-Wl,-ldl'],
                ]
            ]
        }
        shellScript {
            '''
                mpy conan install
                mpy sh ./bootstrap nosubmodule
                mpy sh env PKG_CONFIG_LIBDIR=build/Release \
                    ./configure --enable-dummy --disable-internal-jimtcl
                mpy sh make -j 4
                mpy sh make check
            '''
        }
    }

    job('build-the-docs') {
        resources {
            cpu('1', '1')
            memory('0.5Gi', '0.5Gi')
            fs('1.0Gi', '1.0Gi')
        }
        matrix {
            [
                [
                    image          : ['cpp_ubuntu_22'],
                ]
            ]
        }
        shellScript {
            '''
                mpy conan install
                mpy sh ./bootstrap nosubmodule
                mpy sh env PKG_CONFIG_LIBDIR=build/Release \
                    ./configure --disable-internal-jimtcl
                mpy sh make html
            '''
        }
    }

    deploy('openocd') {
        resources {
            cpu('0.8', '4')
            memory('0.4Gi', '16Gi')
            fs('1.0Gi', '1.5Gi')
        }
        vars { [name: "openocd"] }
        matrix {
            [
                [
                    image    : ['cpp_centos_7', 'cpp_rocky_8', 'cpp_ubuntu_18', 'cpp_ubuntu_20', 'cpp_ubuntu_22'],
                    profile  : ['default'],
                    o_source : ['internal', 'syntacore']
                ], [
                    image    : ['cpp_ubuntu_22'],
                    profile  : ['makepy_sc_mingw', 'mp_armhf'],
                    o_source : ['internal', 'syntacore']
                ]
            ]
        }
    }

    deploy('openocd_testsuite') {
        resources {
            cpu('6.1', '10')
            memory('1.1Gi', '17.1Gi')
            fs('12.5Gi', '22.6Gi')
        }
        vars { [name: "openocd_testsuite"] }
        matrix {
            [
                [
                    image : ['cpp_ubuntu_22'],
                ]
            ]
        }
    }

    job('print_urls') {
        resources {
            cpu('0.2', '4')
            memory('0.1Gi', '8Gi')
            fs('1.0Gi', '1.0Gi')
        }
        dependsOn 'deploy'
        rules { vars ->
            include(vars.name in vars.deploy)
        }
        shellScript {
            '''
                OPTS=""
                if [[ "${VARS_assumeRelease}" ]]; then
                    OPTS="${OPTS} --assume-release"
                fi

                mpy print-package-urls ${OPTS}
            '''
        }
    }
}
