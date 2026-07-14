# ##############################################################################
# vendor/beken/boards/bk7236n/bk7236n-evb/CMakeLists.txt
#
# Licensed to the Apache Software Foundation (ASF) under one or more contributor
# license agreements.  See the NOTICE file distributed with this work for
# additional information regarding copyright ownership.  The ASF licenses this
# file to you under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License.  You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations under
# the License.
#
# ##############################################################################

add_subdirectory(src)

file(GLOB BOARD_LIBS
     ${NUTTX_BOARD_ABS_DIR}/libs/${CONFIG_ARCH_BOARD_CUSTOM_NAME}/*.a)

file(GLOB ARMINO_LIBS
     ${NUTTX_CHIP_ABS_DIR}/bk_idk/armino_as_lib/bk7236n/libs/*.a)

set_property(GLOBAL APPEND PROPERTY NUTTX_EXTRA_LIBRARIES ${BOARD_LIBS}
                                             ${ARMINO_LIBS})

set_property(GLOBAL PROPERTY LD_SCRIPT
             ${NUTTX_BOARD_ABS_DIR}/scripts/ld.script)

add_custom_target(
  nuttx_post_build
  DEPENDS nuttx
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Generate all-app.bin")

add_custom_command(
  TARGET nuttx_post_build
  POST_BUILD
  COMMAND
    ${CMAKE_CURRENT_LIST_DIR}/scripts/postbuild.sh ${CMAKE_BINARY_DIR}
    ${NUTTX_CHIP_ABS_DIR}
    ${CMAKE_SOURCE_DIR}/../vendor/beken/tools/cmake_encrypt_crc
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Generate all-app.bin")
