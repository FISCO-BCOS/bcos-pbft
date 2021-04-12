#------------------------------------------------------------------------------
# Top-level CMake file for bcos-pbft
# ------------------------------------------------------------------------------
# Copyright (C) 2021 bcos-pbft
# SPDX-License-Identifier: Apache-2.0
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.10)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version")

set(BCOS_CMAKE_SCRIPTS "bcos-cmake-scripts")
set(BCOS_CMAKE_SCRIPTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${BCOS_CMAKE_SCRIPTS}" CACHE PATH "The path for the ${BCOS_CMAKE_SCRIPTS} subtree")
set(BCOS_PBFT_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake" CACHE PATH "The cmake path for the library")
list(APPEND CMAKE_MODULE_PATH ${BCOS_CMAKE_SCRIPTS_DIR})
list(APPEND CMAKE_MODULE_PATH ${BCOS_PBFT_CMAKE_DIR})
#init hunter
include(HunterGate)
HunterGate(
    URL "https://github.com/FISCO-BCOS/hunter/archive/5a02ff6cd98a47538e077b3ae07e5142cf1c363a.tar.gz"
    SHA1 "0013602ad7f8d50ec39e5066f06de61f4702c434"
)
project(bcos-pbft VERSION "3.0.0")
set(VERSION_SUFFIX "")

# basic settings
include(Options)
configure_project()
include(CompilerSettings)
include(CopyInclude)

include_directories(${CMAKE_INSTALL_INCLUDEDIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

set(INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
include_directories(${CMAKE_INSTALL_INCLUDEDIR})
include_directories(${INCLUDE_DIR})
include_directories(${INCLUDE_DIR}/${CMAKE_PROJECT_NAME})

set(PROTO_PATH ${PROJECT_SOURCE_DIR})

# install dependencies
include(InstallBcosFrameworkDependencies)

# set the targets
set(PBFT_PROTO_GENERATE_BASE_DIR ${INCLUDE_DIR}/${CMAKE_PROJECT_NAME})
set(CONSENSUS_PROTO_GENERATE_BASE_DIR ${INCLUDE_DIR}/${CMAKE_PROJECT_NAME})

set(BCOS_CONSENSUS_CORE_TARGET "bcos-consensus-core")
set(BCOS_PBFT_TARGET "bcos-pbft")

add_subdirectory(core)
add_subdirectory(pbft)

if (TESTS)
    hunter_add_package(Boost COMPONENTS test)
    enable_testing()
    set(CTEST_OUTPUT_ON_FAILURE TRUE)
endif()

include(InstallConfig)
# install consensus target
install(
    TARGETS ${BCOS_CONSENSUS_CORE_TARGET}
    EXPORT "${TARGETS_EXPORT_NAME}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(
    TARGETS ${BCOS_PBFT_TARGET}
    EXPORT "${TARGETS_EXPORT_NAME}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
# install the include files for hash
include(InstallInclude)

# for doxygen
include(BuildDocs)

# for code coverage
if (COVERAGE)
    include(Coverage)
    config_coverage("'/usr*' '${CMAKE_SOURCE_DIR}/bcos-cmake-scripts*' '${CMAKE_SOURCE_DIR}/test/bcos-test*'")
endif ()
