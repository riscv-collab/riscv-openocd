@Library("jenkins-lib@v2-volatile") _

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

    job('main') {
        resources {
            cpu('16', '16')
            memory('16Gi')
        }
        matrix {
            [[image          : ['cpp_ubuntu_18', 'cpp_ubuntu_20'],
              profile        : ['default'],
              testOpt        : ['--options:host test=True'],
              buildPath      : ['build/Release']],
             [image          : ['cpp_ubuntu_22', 'cpp_centos_7'],
              profile        : ['default'],
              testOpt        : [''],
              buildPath      : ['build/Release']],
             [image          : ['cpp_ubuntu_22'],
              profile        : ['makepy_sc_mingw'],
              testOpt        : [''],
              buildPath      : ['build/Release']]]

        }
        script { vars ->
            Boolean runTest = true as Boolean
            if (vars.releaseString != '') {
                sh(""" ./make.py distr-prep -r "${vars.releaseString}" """)
            }
            if (vars.profile == 'makepy_sc_mingw' || vars.image == 'cpp_centos_7' || vars.image == 'cpp_ubuntu_22') {
                runTest = false
            }
            try {
                sh("./make.py just-config --profile:host ${vars.profile} --build-path ${vars.buildPath} ${vars.testOpt}")
                sh("./make.py build --build-path ${vars.buildPath} --target openocd")
                if (runTest) {
                    sh("./make.py build --build-path ${vars.buildPath} --target OpenOCDTestsOn_spike --parallel 8")
                    sh("./make.py build --build-path ${vars.buildPath} --target RISCVTestsDebug --parallel 8")
                }
            } catch (Exception ex) {
                artifacts.push("build/Release/testing", "artifacts-${vars.profile}-${vars.image}", retention: 'ignore')
                error "wasted!"
            }
        }
    }

    job('deploy') {
        resources {
            cpu('8', '8')
            memory('16Gi')
        }
        dependsOn 'main'
        matrix {
            [[image          : ['cpp_ubuntu_18', 'cpp_centos_7', 'cpp_ubuntu_20', 'cpp_ubuntu_22'],
              profile        : ['default']],
             [image          : ['cpp_ubuntu_22'],
              profile        : ['makepy_sc_mingw']]]
        }
        rules { vars ->
            include(vars.name in vars.deploy)
            include(vars.branch == vars.defaultBranch)
        }
        script { vars ->
            makepy.deployPackage(vars.name, assumeRelease: vars.assumeRelease, profile: vars.profile)
        }
    }

    job('deploy_artifactory') {
        resources {
            cpu('8', '8')
            memory('16Gi')
        }
        dependsOn 'deploy'
        matrix {
            [[image          : ['cpp_ubuntu_18', 'cpp_centos_7', 'cpp_ubuntu_20', 'cpp_ubuntu_22'],
              profile        : ['default']],
             [image          : ['cpp_ubuntu_22'],
              profile        : ['makepy_sc_mingw']]]
        }
        rules { vars ->
            include(vars.name in vars.deploy)
            include(vars.branch == vars.defaultBranch)
        }
        script { vars ->
            String buildPath = 'build/Release' as String
            String testOpt = '' as String
            packageRef = makepy.getConanVars(vars.name, false).packageRef
            if (vars.image == 'cpp_ubuntu_18' || vars.image == 'cpp_ubuntu_20') {
                testOpt = '--options:host test=True'
            }
            sh("./make.py just-config --profile:host ${vars.profile} --build-path ${buildPath} ${testOpt}")
            sh(""" ./make.py conan install \
                    --profile:host ${vars.profile} --remote syntacore --requires "${packageRef}" --lockfile-partial \
                    --output-folder build/deploy --deployer .makepy/support/utils/conan_the_deployer.py """)
            withCredentials([string(credentialsId: 'artifactory_cicdsc_api_key', variable: 'ART_API_KEY')]) {
                sh(""" ./make.py sh .makepy/support/utils/upload_development_build.sh \
                    \$(find build/deploy -maxdepth 1 -name '*bundle*') $ART_API_KEY """)
            }
        }
    }

    job('deploy_tag') {
        dependsOn 'deploy'
        matrix {
            [[image: ['cpp_ubuntu_18']]]
        }
        rules { vars ->
            include(vars.name in vars.deploy)
            include(vars.branch == vars.defaultBranch)
        }
        script { vars ->
            makepy.deployTag(vars.name, assumeRelease: vars.assumeRelease)
        }
    }
}
