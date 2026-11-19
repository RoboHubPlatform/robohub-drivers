#
# robohub-drivers' tests, registered with CTest so that `rho test` runs them
# (tests/tests.yml declares this directory as a ctest suite).
#
# This is a test project and nothing else: the collection itself is not built
# by CMake, and nothing here compiles the drivers. It registers:
#
#   host-tests      tests/run.sh: the host tests in tests/host-tests.plan, built
#                   and run by the ecosystem's runner in robohub
#                   (shared/tests/run-host-tests.sh) against the sibling
#                   checkout of robohub-common, which holds the interfaces and
#                   the HAL the drivers are written against.
#
# The sibling checkouts are found under ROBOHUB_WORKSPACE, the directory the
# manifests name them under, or beside this repository when it is unset.
#
# @project RoboHub Platform
# @license MIT
#

cmake_minimum_required(VERSION 3.16)

project(robohub-drivers-tests NONE)

enable_testing()

get_filename_component(ROBOHUB_DRIVERS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)

if(DEFINED ENV{ROBOHUB_WORKSPACE} AND NOT "$ENV{ROBOHUB_WORKSPACE}" STREQUAL "")
    set(ROBOHUB_WORKSPACE_DIR "$ENV{ROBOHUB_WORKSPACE}")
else()
    get_filename_component(ROBOHUB_WORKSPACE_DIR "${ROBOHUB_DRIVERS_ROOT}" DIRECTORY)
endif()

find_program(ROBOHUB_SH sh REQUIRED)

foreach(sibling robohub robohub-common)
    if(NOT EXISTS "${ROBOHUB_WORKSPACE_DIR}/${sibling}")
        message(FATAL_ERROR
            "robohub-drivers' host tests build against ${sibling}, which is not at "
            "${ROBOHUB_WORKSPACE_DIR}/${sibling}.\n"
            "  Check it out beside this repository, or set ROBOHUB_WORKSPACE to the "
            "directory the checkouts are in.")
    endif()
endforeach()

# The host tests. Their build directory is this one, so a run leaves nothing in
# the system's temporary directory.
add_test(NAME host-tests COMMAND "${ROBOHUB_SH}" "${ROBOHUB_DRIVERS_ROOT}/tests/run.sh")
set_tests_properties(host-tests PROPERTIES ENVIRONMENT
    "ROBOHUB_COMMON_DIR=${ROBOHUB_WORKSPACE_DIR}/robohub-common;ROBOHUB_ORCHESTRATOR_DIR=${ROBOHUB_WORKSPACE_DIR}/robohub;TMPDIR=${CMAKE_CURRENT_BINARY_DIR}")
