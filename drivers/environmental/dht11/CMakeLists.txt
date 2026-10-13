cmake_minimum_required(VERSION 3.16)
project(robohub-driver-dht11 C)

# ---------------------------------------------------------------------------
# Locating a sibling module
# ---------------------------------------------------------------------------
# The driver implements driver interfaces and reaches the device through the
# single-wire HAL, so it links the module that declares them. A path is never
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
add_library(robohub-driver-dht11 STATIC
    src/dht11.c
)

# ---------------------------------------------------------------------------
# Public include layout
# ---------------------------------------------------------------------------
# A consumer writes the path that says where the driver sits in the ecosystem —
# robohub/drivers/environmental/dht11.h — while the header itself stays flat in
# inc/dht11.h. The namespaced path is a generated copy in the build tree, put on
# the path through this target's interface.
set(ROBOHUB_DRIVER_DHT11_NAMESPACE "robohub/drivers/environmental")
set(ROBOHUB_DRIVER_DHT11_INCLUDE_GEN
    "${CMAKE_CURRENT_BINARY_DIR}/include/${ROBOHUB_DRIVER_DHT11_NAMESPACE}")
file(MAKE_DIRECTORY "${ROBOHUB_DRIVER_DHT11_INCLUDE_GEN}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/inc/dht11.h"
    "${ROBOHUB_DRIVER_DHT11_INCLUDE_GEN}/dht11.h"
    COPYONLY)

target_include_directories(robohub-driver-dht11 PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${ROBOHUB_COMMON_DIR}/include
)

target_link_libraries(robohub-driver-dht11 PUBLIC robohub-common)
target_compile_features(robohub-driver-dht11 PUBLIC c_std_11)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(FILES module.yml Kconfig DESTINATION share/robohub-driver-dht11)
install(FILES inc/dht11.h DESTINATION include/robohub/drivers/environmental)
install(TARGETS robohub-driver-dht11
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
