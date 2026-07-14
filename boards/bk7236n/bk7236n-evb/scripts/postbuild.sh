#!/usr/bin/env bash
#
# vendor/beken/boards/bk7236n/bk7236n-evb/scripts/postbuild.sh
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#

set -euo pipefail

BINARY_DIR="$1"
CHIP_DIR="$2"
ENCRYPT_TOOL="$3"

"${ENCRYPT_TOOL}" -crc "${BINARY_DIR}/nuttx.bin"
cat "${CHIP_DIR}/bootloader.bin" "${BINARY_DIR}/nuttx_crc.bin" > "${BINARY_DIR}/all-app.bin"
