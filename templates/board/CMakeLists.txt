cmake_minimum_required(VERSION 3.16)
project(robohub-board-{{name}}-module C)

# The board's support package touches the driver and, through it, the common
# module that declares the interfaces. A sibling is found by name, searched
# upward, and fetched only as a last resort.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/deps.cmake")
endif()

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
  set(dir "${CMAKE_CURRENT_SOURCE_DIR}")
  foreach(level RANGE 8)
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
robohub_locate_module(robohub-driver-{{name}} ROBOHUB_DRIVER_{{name}}_DIR)
if(NOT ROBOHUB_COMMON_DIR OR NOT ROBOHUB_DRIVER_{{name}}_DIR)
  message(FATAL_ERROR "robohub-common or robohub-driver-{{name}} not found")
endif()

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  if(NOT TARGET robohub-common)
    add_subdirectory("${ROBOHUB_COMMON_DIR}" robohub-common)
  endif()
  if(NOT TARGET robohub-driver-{{name}})
    add_subdirectory("${ROBOHUB_DRIVER_{{name}}_DIR}" robohub-driver-{{name}})
  endif()
endif()

add_library(robohub-board-{{name}}-module STATIC bsp/bsp.c)

target_include_directories(robohub-board-{{name}}-module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/bsp
    ${ROBOHUB_COMMON_DIR}/include
)
target_link_libraries(robohub-board-{{name}}-module PUBLIC robohub-driver-{{name}} robohub-common)
target_compile_features(robohub-board-{{name}}-module PUBLIC c_std_11)

install(FILES module.yml Kconfig DESTINATION share/robohub-board-{{name}}-module)
install(DIRECTORY bsp/ DESTINATION include)
install(TARGETS robohub-board-{{name}}-module ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
