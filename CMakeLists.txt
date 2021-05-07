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

if (NOT DEFINED URL_BASE)
    set(URL_BASE "github.com")
    # set(URL_BASE "github.com.cnpmjs.org")
endif()

include(FetchContent)
set(BCOS_CMAKE_SCRIPTS_DIR ${CMAKE_CURRENT_BINARY_DIR}/bcos-cmake-scripts)
FetchContent_Declare(bcos-cmake-scripts
    GIT_REPOSITORY https://${URL_BASE}/FISCO-BCOS/bcos-cmake-scripts.git
    GIT_TAG dev
    SOURCE_DIR ${BCOS_CMAKE_SCRIPTS_DIR}
)
FetchContent_MakeAvailable(bcos-cmake-scripts)

set(BCOS_PBFT_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake" CACHE PATH "The cmake path for the library")
list(APPEND CMAKE_MODULE_PATH ${BCOS_CMAKE_SCRIPTS_DIR})
list(APPEND CMAKE_MODULE_PATH ${BCOS_PBFT_CMAKE_DIR})
#init hunter
include(HunterGate)
HunterGate(
    URL "https://github.com/FISCO-BCOS/hunter/archive/8c81ea454c0c268b257da1fe3e3f2f69bd59a67d.tar.gz"
    SHA1 "13292adf48c4c053ff2ceb7173631a296c08d1ac"
    FILEPATH "${BCOS_PBFT_CMAKE_DIR}/config.cmake"
)
project(bcos-pbft VERSION "3.0.0")
set(VERSION_SUFFIX "")

# basic settings
include(Options)
configure_project()
include(CompilerSettings)

include_directories(${CMAKE_INSTALL_INCLUDEDIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(PROTO_PATH ${PROJECT_SOURCE_DIR})

# install dependencies
include(InstallBcosFrameworkDependencies)

# for proto generation
set(PBFT_PROTO_GENERATE_BASE_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CONSENSUS_PROTO_GENERATE_BASE_DIR ${CMAKE_CURRENT_BINARY_DIR})

set(BCOS_CONSENSUS_CORE_TARGET "bcos-consensus-core")
set(BCOS_PBFT_TARGET "bcos-pbft")

add_subdirectory(core)
add_subdirectory(pbft)

if (TESTS)
    # fetch bcos-test
    set(BCOS_TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/test/bcos-test)
    FetchContent_Declare(bcos-test
    GIT_REPOSITORY https://${URL_BASE}/FISCO-BCOS/bcos-test.git
    GIT_TAG dev
    SOURCE_DIR ${BCOS_TEST_DIR})
    FetchContent_MakeAvailable(bcos-test)
    
    enable_testing()
    set(CTEST_OUTPUT_ON_FAILURE TRUE)
    add_subdirectory(test)
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
    config_coverage("'/usr*' '${CMAKE_CURRENT_SOURCE_DIR}/bcos-cmake-scripts*' '${CMAKE_CURRENT_SOURCE_DIR}/test/bcos-test*'")
endif ()
