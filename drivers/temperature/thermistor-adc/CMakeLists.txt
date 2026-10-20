cmake_minimum_required(VERSION 3.16)
project(robohub-driver-thermistor-adc C)

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

add_library(robohub-driver-thermistor-adc STATIC src/thermistor_adc.c)

set(NS "robohub/drivers/temperature")
set(GEN "${CMAKE_CURRENT_BINARY_DIR}/include/${NS}")
file(MAKE_DIRECTORY "${GEN}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/inc/thermistor_adc.h" "${GEN}/thermistor_adc.h" COPYONLY)

target_include_directories(robohub-driver-thermistor-adc PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${ROBOHUB_COMMON_DIR}/include
)
target_link_libraries(robohub-driver-thermistor-adc PUBLIC robohub-common)
target_link_libraries({name} PUBLIC m)
target_compile_features(robohub-driver-thermistor-adc PUBLIC c_std_11)

install(FILES module.yml Kconfig DESTINATION share/robohub-driver-thermistor-adc)
install(FILES inc/thermistor_adc.h DESTINATION include/robohub/drivers/temperature)
install(TARGETS robohub-driver-thermistor-adc ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
