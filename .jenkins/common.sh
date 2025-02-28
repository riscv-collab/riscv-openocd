set -e
set -u

declare -a VARS_as_conan_args
VARS_as_conan_args=(
  -pr ${VARS_profile:=default}
  -s openocd/*:build_type=${VARS_s_build_type:=Release}
  -o openocd/*:source=${VARS_o_source:=internal}
  -o openocd/*:sanitize=${VARS_o_sanitize:=disable}
)

installOpenOCDto() {
  ocd_install_path=$1
  mpy conan build ${VARS_as_conan_args[@]}
  mpy sh env DESTDIR="${ocd_install_path}" \
    .jenkins/destdir_wrap.sh \
    make \
    -C build/$VARS_s_build_type \
    install -j
}

configureTestsuite() {
  workdir=$1
  ocd_install="${workdir}/openocd_install"

  installOpenOCDto $ocd_install

  mpy conan install ${VARS_as_conan_args[@]} \
    --name openocd_testsuite \
    --output-folder $workdir

  mpy config \
    ${VARS_tests_adapter_info:+--tests-adapter-info ${VARS_tests_adapter_info}} \
    ${VARS_tests_valgrind_path:+--tests-valgrind-path ${VARS_tests_valgrind_path}} \
    --build-path "${workdir}" \
    --openocd-install "${ocd_install}"
}

testsBuild() {
  workdir=$1

  mpy build --build-path ${workdir} --target=$VARS_target
}

runTests() {
  workdir=$1

  configureTestsuite $workdir

  testsBuild $workdir
}

buildTransferableTestsuite() {
  workdir=$1

  configureTestsuite $workdir

  mpy build --build-path ${workdir} \
    --target transferable_testsuite
}

runTransferableTests() {
  workdir=$1

  buildTransferableTestsuite $workdir

  testsBuild $workdir
}
