cmake_minimum_required(VERSION 3.16)
project(robohub-board-ds18b20-temperature-module C)

# ---------------------------------------------------------------------------
# Locate what the board joins
# ---------------------------------------------------------------------------
# The board's support package touches the driver and, through it, the common
# module that declares the interfaces. Built as a dependency the generated
# project adds both itself; this is for a standalone build of the module.
if(NOT ROBOHUB_COMMON_DIR)
  if(EXISTS "/home/miraur/projects/robohub-common")
    set(ROBOHUB_COMMON_DIR "/home/miraur/projects/robohub-common")
  elseif(DEFINED ENV{ROBOHUB_COMMON_DIR})
    set(ROBOHUB_COMMON_DIR "$ENV{ROBOHUB_COMMON_DIR}")
  else()
    message(FATAL_ERROR "robohub-common not found. Set ROBOHUB_COMMON_DIR or place it beside this checkout.")
  endif()
endif()

# The driver is a sibling inside this repository, two levels up and back down
# into drivers/<category>/<device>.
if(NOT ROBOHUB_DS18B20_DIR)
  get_filename_component(ROBOHUB_DS18B20_DIR
      "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/temperature/ds18b20" ABSOLUTE)
endif()
message(STATUS "[robohub-board-ds18b20-temperature-module] driver at ${ROBOHUB_DS18B20_DIR}")

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  if(NOT TARGET robohub-common)
    add_subdirectory("${ROBOHUB_COMMON_DIR}" robohub-common)
  endif()
  if(NOT TARGET robohub-driver-ds18b20)
    add_subdirectory("${ROBOHUB_DS18B20_DIR}" robohub-driver-ds18b20)
  endif()
endif()

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
add_library(robohub-board-ds18b20-temperature-module STATIC
    bsp/bsp.c
)

target_include_directories(robohub-board-ds18b20-temperature-module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp
    ${ROBOHUB_COMMON_DIR}/include
)

target_link_libraries(robohub-board-ds18b20-temperature-module PUBLIC
    robohub-driver-ds18b20
    robohub-common
)
target_compile_features(robohub-board-ds18b20-temperature-module PUBLIC c_std_11)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(FILES module.yml Kconfig DESTINATION share/robohub-board-ds18b20-temperature-module)
install(DIRECTORY bsp/ DESTINATION include)
install(TARGETS robohub-board-ds18b20-temperature-module
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
