cmake_minimum_required(VERSION 3.16)
project(robohub-board-dht11-module C)

# ---------------------------------------------------------------------------
# Locating a sibling module
# ---------------------------------------------------------------------------
# The board's support package touches the driver and, through it, the common
# module that declares the interfaces. A path is never hard-coded: a variable a
# parent build already set (rho writes one for every dependency) wins, then the
# environment, then a sibling checkout somewhere above, and finally a fetch.
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
  # A sibling checkout, searched upward: a module may sit nested — a board and a
  # driver inside one collection — with the sibling several levels above it.
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
robohub_locate_module(robohub-driver-dht11 ROBOHUB_DRIVER_DHT11_DIR)
if(NOT ROBOHUB_COMMON_DIR OR NOT ROBOHUB_DRIVER_DHT11_DIR)
  message(FATAL_ERROR "robohub-common or robohub-driver-dht11 not found and could not be fetched")
endif()
message(STATUS "[${PROJECT_NAME}] driver at ${ROBOHUB_DRIVER_DHT11_DIR}")

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  if(NOT TARGET robohub-common)
    add_subdirectory("${ROBOHUB_COMMON_DIR}" robohub-common)
  endif()
  if(NOT TARGET robohub-driver-dht11)
    add_subdirectory("${ROBOHUB_DRIVER_DHT11_DIR}" robohub-driver-dht11)
  endif()
endif()

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
add_library(robohub-board-dht11-module STATIC
    bsp/bsp.c
)

target_include_directories(robohub-board-dht11-module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp
    ${ROBOHUB_COMMON_DIR}/include
)

target_link_libraries(robohub-board-dht11-module PUBLIC
    robohub-driver-dht11
    robohub-common
)
target_compile_features(robohub-board-dht11-module PUBLIC c_std_11)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(FILES module.yml Kconfig DESTINATION share/robohub-board-dht11-module)
install(DIRECTORY bsp/ DESTINATION include)
install(TARGETS robohub-board-dht11-module
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
