cmake_minimum_required(VERSION 3.16)
project(robohub-driver-ds18b20 C)

# ---------------------------------------------------------------------------
# Locate robohub-common
# ---------------------------------------------------------------------------
# The driver implements a driver interface and reaches the device through HAL
# interfaces, so it links the module that declares them. Built as a dependency
# the generated project adds that module itself; this is for a standalone build.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/deps.cmake")
endif()

if(NOT ROBOHUB_COMMON_DIR)
  if(EXISTS "/home/miraur/projects/robohub-common")
    set(ROBOHUB_COMMON_DIR "/home/miraur/projects/robohub-common")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../../robohub-common")
    get_filename_component(ROBOHUB_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../robohub-common" ABSOLUTE)
  elseif(DEFINED ENV{ROBOHUB_COMMON_DIR})
    set(ROBOHUB_COMMON_DIR "$ENV{ROBOHUB_COMMON_DIR}")
  else()
    message(FATAL_ERROR "robohub-common not found. Set ROBOHUB_COMMON_DIR or place it beside this checkout.")
  endif()
endif()
message(STATUS "[robohub-driver-ds18b20] robohub-common at ${ROBOHUB_COMMON_DIR}")

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

target_include_directories(robohub-driver-ds18b20 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${ROBOHUB_COMMON_DIR}/include
)

target_link_libraries(robohub-driver-ds18b20 PUBLIC robohub-common)
target_compile_features(robohub-driver-ds18b20 PUBLIC c_std_11)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(FILES module.yml Kconfig DESTINATION share/robohub-driver-ds18b20)
install(DIRECTORY inc/ DESTINATION include)
install(TARGETS robohub-driver-ds18b20
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
