#!/usr/bin/env bash
# Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# SPDX-License-Identifier: Apache-2.0
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


#
# Some shell-scripts are used to handle machine specific things. For example deploying new binaries
# to a running image. To do so the ip_address of the specific machine must be known.
#
# Instead of hard coding those values in each script, this script shall be used as a central point
# to store those values.
#

set -e

#
# Constants indicating the machine options and number of network configurations per machine
readonly MACHINE_OPTIONS=('m_pp' 'm_high_pp')
readonly NUMBER_OF_XPAD_CONFIGURATIONS=9
#
# Given on the variable ${OPT_MACHINE}, network specific variables will be populated
get_network_configuration() {
    scriptpath="$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )"
    commandOutput="$(python "${scriptpath}"/machine_specific_config.py -t "${OPT_MACHINE}")"
    configArray=($commandOutput)
    if [ "${#configArray[@]}" -eq $NUMBER_OF_XPAD_CONFIGURATIONS ]; then
        export host_ip_address_1=${configArray[0]}
        export target_ip_address_1=${configArray[1]}
        export broadcast_1=${configArray[2]}
        export host_ip_address_2=${configArray[3]}
        export target_ip_address_2=${configArray[4]}
        export broadcast_2=${configArray[5]}
        export host_ip_address_3=${configArray[6]}
        export target_ip_address_3=${configArray[7]}
        export broadcast_3=${configArray[8]}
    else
        (>&2 echo "Incorrect number of network configurations for machine '${OPT_MACHINE}'."); exit -1
    fi
}

#
# Checks if the element in in array
containsElement () {
  local e match="$1"
  shift
  for e; do [[ "$e" == "$match" ]] && return 0; done
  return 1
}

#
# Given on the variable ${OPT_MACHINE}, network specific variables will be populated
calculate_network() {
    if containsElement "${OPT_MACHINE}" "${MACHINE_OPTIONS[@]}"; then
        get_network_configuration
    else
        (>&2 echo "Unrecognized machine '${OPT_MACHINE}'."); exit -1
    fi
}
