# ------------------------------------------------------------------------------
# Copyright (C) 2021 FISCO BCOS.
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
# ------------------------------------------------------------------------------
# File: BuildDocs.cmake
# Function: Documentation build cmake
# ------------------------------------------------------------------------------
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
if (NOT BCOS_DOXYGEN_DIR)
    set(BCOS_DOXYGEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bcos-cmake-scripts")
endif()
# Requirements: doxygen graphviz
  set(doxyfile_in "${BCOS_DOXYGEN_DIR}/.Doxyfile.in")
  set(doxyfile "${BCOS_DOXYGEN_DIR}/Doxyfile")
  configure_file(${doxyfile_in} ${doxyfile} @ONLY)
# Add doc target
add_custom_target(doc COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                       COMMENT "Generating documentation with Doxygen..." VERBATIM)
elseif()
  message(WARNING "Doxygen is needed to build the documentation. Please install doxygen and graphviz")
endif()