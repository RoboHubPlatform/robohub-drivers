cmake_minimum_required(VERSION 3.16)
project(robohub-driver-infrared-pwm C)

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
if(NOT ROBOHUB_COMMON_DIR)
  message(FATAL_ERROR "robohub-common not found")
endif()

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  if(NOT TARGET robohub-common)
    add_subdirectory("${ROBOHUB_COMMON_DIR}" robohub-common)
  endif()
endif()

add_library(robohub-driver-infrared-pwm STATIC src/infrared_pwm.c)

set(NS "robohub/drivers/output")
set(GEN "${CMAKE_CURRENT_BINARY_DIR}/include/${NS}")
file(MAKE_DIRECTORY "${GEN}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/inc/infrared_pwm.h" "${GEN}/infrared_pwm.h" COPYONLY)

target_include_directories(robohub-driver-infrared-pwm PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${ROBOHUB_COMMON_DIR}/include
)
target_link_libraries(robohub-driver-infrared-pwm PUBLIC robohub-common)
target_compile_features(robohub-driver-infrared-pwm PUBLIC c_std_11)

install(FILES module.yml Kconfig DESTINATION share/robohub-driver-infrared-pwm)
install(FILES inc/infrared_pwm.h DESTINATION include/${NS})
install(TARGETS robohub-driver-infrared-pwm ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
