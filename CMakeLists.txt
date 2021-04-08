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
    URL "https://github.com/FISCO-BCOS/hunter/archive/3a6e68dd1359d27de88c5a1c3306445ca79aa806.tar.gz"
    SHA1 "34d3d21bf6ef110c45b36ba3d579b955481faab5"
)
project(bcos-pbft VERSION "3.0.0")
set(VERSION_SUFFIX "")

# basic settings
include(Options)
configure_project()
include(CompilerSettings)

include_directories(${CMAKE_INSTALL_INCLUDEDIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
# install dependencies
include(InstallBcosFrameworkDependencies)

# for doxygen
include(BuildDocs)

# for code coverage
if (COVERAGE)
    include(Coverage)
    config_coverage("'/usr*' '${CMAKE_SOURCE_DIR}/bcos-cmake-scripts*' '${CMAKE_SOURCE_DIR}/test/bcos-test*'")
endif ()
