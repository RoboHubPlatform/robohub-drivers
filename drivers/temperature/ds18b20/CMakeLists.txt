cmake_minimum_required(VERSION 3.16)
project(robohub-driver-ds18b20 C)

# ---------------------------------------------------------------------------
# Locating a sibling module
# ---------------------------------------------------------------------------
# The driver implements a driver interface and reaches the device through HAL
# interfaces, so it links the module that declares them. A path is never
# hard-coded: a variable the parent build already set (rho writes one for every
# dependency) wins, then the environment, then a sibling checkout somewhere
# above, and finally the module's repository, fetched.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/deps.cmake")
endif()

# robohub_locate_module(<name> <out-var>): find a sibling module and set the
# variable to its directory. `<name>` is the module's name, so the variable is
# its upper-case form with `_DIR` appended.
function(robohub_locate_module module out_var)
  string(TOUPPER "${module}" upper)
  string(REPLACE "-" "_" upper "${upper}")
  if(${out_var})
    return()
  endif()
  if(DEFINED ENV{${upper}_DIR})
    set(${out_var} "$ENV{${upper}_DIR}" PARENT_SCOPE)
    return()
  endif()
  # A sibling checkout, searched upward: a module may sit nested — a driver
  # inside a collection — with the sibling several levels above it.
  set(dir "${CMAKE_CURRENT_SOURCE_DIR}")
  foreach(level RANGE 6)
    get_filename_component(parent "${dir}/.." ABSOLUTE)
    if(EXISTS "${parent}/${module}/CMakeLists.txt")
      set(${out_var} "${parent}/${module}" PARENT_SCOPE)
      return()
    endif()
    set(dir "${parent}")
  endforeach()
  include(FetchContent)
  message(STATUS "[${PROJECT_NAME}] fetching ${module}")
  FetchContent_Declare(${module}
    GIT_REPOSITORY "https://github.com/RoboHubPlatform/${module}.git"
    GIT_TAG main GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
  FetchContent_MakeAvailable(${module})
  set(${out_var} "${${module}_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

robohub_locate_module(robohub-common ROBOHUB_COMMON_DIR)
if(NOT ROBOHUB_COMMON_DIR)
  message(FATAL_ERROR "robohub-common not found and could not be fetched")
endif()
message(STATUS "[${PROJECT_NAME}] robohub-common at ${ROBOHUB_COMMON_DIR}")

# Added only when this driver *is* the project. Built as a dependency — the
# usual case — the generated project adds robohub-common itself, and adding it
# here too would be two targets of one name.
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  add_subdirectory("${ROBOHUB_COMMON_DIR}" robohub-common)
endif()

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
add_library(robohub-driver-ds18b20 STATIC
    src/ds18b20.c
)

# ---------------------------------------------------------------------------
# Public include layout
# ---------------------------------------------------------------------------
# A consumer writes the path that says where the driver sits in the ecosystem —
# robohub/drivers/temperature/ds18b20.h — while the header itself stays flat in
# inc/ds18b20.h. The namespaced path is a generated copy in the build tree, put
# on the path through this target's interface: the source keeps the short
# layout, and what a consumer sees keeps the long one.
set(ROBOHUB_DRIVER_DS18B20_NAMESPACE "robohub/drivers/temperature")
set(ROBOHUB_DRIVER_DS18B20_INCLUDE_GEN
    "${CMAKE_CURRENT_BINARY_DIR}/include/${ROBOHUB_DRIVER_DS18B20_NAMESPACE}")
file(MAKE_DIRECTORY "${ROBOHUB_DRIVER_DS18B20_INCLUDE_GEN}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/inc/ds18b20.h"
    "${ROBOHUB_DRIVER_DS18B20_INCLUDE_GEN}/ds18b20.h"
    COPYONLY)

target_include_directories(robohub-driver-ds18b20 PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${ROBOHUB_COMMON_DIR}/include
)

target_link_libraries(robohub-driver-ds18b20 PUBLIC robohub-common)
target_compile_features(robohub-driver-ds18b20 PUBLIC c_std_11)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(FILES module.yml Kconfig DESTINATION share/robohub-driver-ds18b20)
# Installed under the name a consumer includes, which is the module's place in
# the ecosystem and not the file's place in this directory.
install(FILES inc/ds18b20.h DESTINATION include/robohub/drivers/temperature)
install(TARGETS robohub-driver-ds18b20
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
