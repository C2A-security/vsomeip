# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 2.8)

set (VSOMEIP_NAME vsomeip3)
set (VSOMEIP_COMPAT_NAME vsomeip)

set(EXAMPLE_CONFIG_FILES
    "../config/vsomeip.json"
    "../config/vsomeip-local.json"
    "../config/vsomeip-tcp-client.json"
    "../config/vsomeip-tcp-service.json"
    "../config/vsomeip-udp-client.json"
    "../config/vsomeip-udp-service.json"
)

if (VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS EQUAL 1)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS")
endif ()
if (CMAKE_BUILD_TYPE EQUAL "Debug")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
endif ()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DILYA") # ilya come here

set(DL_LIBRARY "dl")

find_package( Boost 1.55 COMPONENTS system thread filesystem REQUIRED )
include_directories( ${Boost_INCLUDE_DIR} )
if(Boost_FOUND)
  if(Boost_LIBRARY_DIR)
    MESSAGE( STATUS "Boost_LIBRARY_DIR not empty using it: ${Boost_LIBRARY_DIR}" )
  else()
    if(BOOST_LIBRARYDIR)
      MESSAGE( STATUS "Boost_LIBRARY_DIR empty but BOOST_LIBRARYDIR is set setting Boost_LIBRARY_DIR to: ${BOOST_LIBRARYDIR}" )
      set(Boost_LIBRARY_DIR ${BOOST_LIBRARYDIR})
    endif()
  endif()
else()
  MESSAGE( STATUS "Boost was not found!")
endif()

# cmake 3.15 introduced a new variable and a new format for the old one
if (DEFINED Boost_VERSION_MACRO)
  set(VSOMEIP_BOOST_VERSION ${Boost_VERSION_MACRO})
else()
  set(VSOMEIP_BOOST_VERSION ${Boost_VERSION})
endif()

message( STATUS "Using boost version: ${VSOMEIP_BOOST_VERSION}" )
if (${VSOMEIP_BOOST_VERSION} GREATER 107300)
message( ERROR "boost version ${VSOMEIP_BOOST_VERSION} is not (yet) supported. Latest supported version is 1.72.0" )
elseif(${VSOMEIP_BOOST_VERSION} GREATER 106999)
set(VSOMEIP_BOOST_HELPER implementation/helper/1.70)
elseif(${VSOMEIP_BOOST_VERSION} GREATER 106599)
set(VSOMEIP_BOOST_HELPER implementation/helper/1.66)
else()
set(VSOMEIP_BOOST_HELPER implementation/helper/1.55)
endif()

include_directories(
    ../interface
    ${VSOMEIP_BOOST_HELPER}
#    ${DLT_INCLUDE_DIRS}
)

include_directories(..)

# message("CXX=${CMAKE_CXX_FLAGS}")
# STRING(REPLACE "-O2" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
# add_definitions(-O0)
# message("XXX=${CMAKE_CXX_FLAGS}")
# message("C  =${CMAKE_C_FLAGS}")
# Examples

link_directories(
    ../../local/lib
)

add_executable(request-sample request-sample.cpp ${EXAMPLE_CONFIG_FILES})
target_link_libraries(request-sample ${VSOMEIP_NAME} ${VSOMEIP_NAME}-cfg ${VSOMEIP_NAME}-sd ${Boost_LIBRARIES} ${DL_LIBRARY})

add_executable(response-sample response-sample.cpp ${EXAMPLE_CONFIG_FILES})
target_link_libraries(response-sample ${VSOMEIP_NAME} ${VSOMEIP_NAME}-cfg ${VSOMEIP_NAME}-sd ${Boost_LIBRARIES} ${DL_LIBRARY})

add_executable(subscribe-sample subscribe-sample.cpp ${EXAMPLE_CONFIG_FILES})
target_link_libraries(subscribe-sample ${VSOMEIP_NAME} ${VSOMEIP_NAME}-cfg ${VSOMEIP_NAME}-sd ${Boost_LIBRARIES} ${DL_LIBRARY})

add_executable(notify-sample notify-sample.cpp ${EXAMPLE_CONFIG_FILES})
target_link_libraries(notify-sample ${VSOMEIP_NAME} ${VSOMEIP_NAME}-cfg ${VSOMEIP_NAME}-sd ${Boost_LIBRARIES} ${DL_LIBRARY})

# get_target_property(COPT request-sample COMPILE_OPTIONS)
# list(REMOVE_ITEM COPT -O2)
# list(REMOVE_ITEM COPT -DNDEBUG)
# set_target_properties(request-sample PROPERTIES COMPILE_OPTIONS "${COPT}")

# add_dependencies(examples request-sample response-sample subscribe-sample notify-sample)

# install (
#     TARGETS request-sample response-sample subscribe-sample notify-sample
#     RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
# )

###################################################################################################
