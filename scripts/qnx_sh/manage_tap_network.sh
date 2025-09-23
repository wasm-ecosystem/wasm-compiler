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


set -euo pipefail

LIB_DIR=$(dirname "$0")


# A fake enum to distinguish between creating an interface or removing it.
# Those values act as magic-string, since bash does not support enums.
readonly CREATE_VALUE="create"
readonly DELETE_VALUE="delete"

readonly DEF_MACHINE="m_pp"

# Setup dependent sh_library
# shellcheck source=./machine_specific_config.sh
source "${LIB_DIR}/machine_specific_config.sh"

# parse_opts(command-line options)
#
# Parse command-line options:
#   - If unrecognized option hit, print message and exit with error
#   - Set OPT_ variables with command-line given values for options (initialize
#     with default values)
#
# Returns 0 (success) if no error occurs, 1 otherwise.
parse_opts () {
    # Set defaults
    OPT_MACHINE=${DEF_MACHINE}
    OPT_COMMAND="create"
    OPT_NS_NAME=""

    # Parse options
    local opt
    while [[ $# -gt 0 ]]; do
    opt="$1"
    case ${opt} in
        -m|--machine)
            shift
            export OPT_MACHINE="$1"; shift
        ;;
        -c|--create)
            OPT_COMMAND="${CREATE_VALUE}"
            shift
            if [[ $# -gt 0 ]]; then
                OPT_NS_NAME="$1"
                shift
            fi
        ;;
        -d|--delete)
            OPT_COMMAND="${DELETE_VALUE}"
            shift
            if [[ $# -gt 0 ]]; then
                OPT_NS_NAME="$1"
                shift
            fi
        ;;
        -h|--help)
            print_usage; exit 0;
        ;;
        *)
            echo "Unrecognized option '${opt}'"; exit 1
        ;;
    esac
    done
}

# Print script usage.
print_usage () {
    echo "Usage:"
    echo "    $0"
    echo "            -m | --machine    machine for which the network should be configured (m_pp or m_high_pp)"
    echo "            -c | --create     Select this option if the network setup shall be created"
    echo "            -d | --delete     Select this option if the network setup shall be removed"
    echo
}

# check_for_installed_tools
#
# Checks if all tools that are needed for executing this script with the intended outcome,
# are installed on the executing machine.
#
# Returns 0 (success) if no errors occurred 1 otherwise
check_for_installed_tools() {
    readonly IFCONFIG=$(which ip 2> /dev/null)
    if [ ! -x "${IFCONFIG}" ]; then
        echo "${IFCONFIG} cannot be executed. ip command not in path or not installed"
        exit 1
    fi
}

# create_network_interface
#
# Creates a new tap device using tunctl. Afterwards it configures the new network interface with
# the correct ip-address and the correct routing information.
#
# Returns 0 (success) if no errors occurred 1 otherwise
create_network_interface() {
    # -b for output only device name
    # -g for the group the device should be created for
    NS_NAME=$1

    if [ -z "$NS_NAME" ]; then
        IP_NETNS="sudo"
    else
        IP_NETNS="sudo ${IFCONFIG} netns exec ${NS_NAME}"
    fi

    if [ ! -z "$NS_NAME" ]; then
        sudo "${IFCONFIG}" netns add "${NS_NAME}"
    fi

    readonly TAP_1=tap0
    ${IP_NETNS} "${IFCONFIG}" tuntap add mode tap ${TAP_1}

    if ! ${IP_NETNS} "${IFCONFIG}" link set dev ${TAP_1} up; then
        echo "Failed to bring up ${TAP_1}"
        exit 1
    fi
    echo "Created tap interface: ${TAP_1}"

    readonly VLAN_1=73

    if ! ${IP_NETNS} "${IFCONFIG}" link add link ${TAP_1} name ${TAP_1}.${VLAN_1} type vlan id ${VLAN_1}; then
        echo "Failed to create vlan interface: ${TAP_1}.${VLAN_1}"
    else
      if ! ${IP_NETNS} "${IFCONFIG}" addr add "${host_ip_address_1:?}"/24 broadcast "${broadcast_1:?}" dev ${TAP_1}.${VLAN_1}; then
          echo "Failed to set up IP addressing on ${TAP_1}.${VLAN_1}"
      else
        if ! ${IP_NETNS} "${IFCONFIG}" link set dev ${TAP_1}.${VLAN_1} up; then
            echo "Failed to bring up ${TAP_1}.${VLAN_1}"
        else
            echo "Created vlan interface: ${TAP_1}.${VLAN_1}"
        fi
      fi
    fi

    # second tap device
    readonly TAP_2=tap1
    ${IP_NETNS} "${IFCONFIG}" tuntap add mode tap ${TAP_2}
    if ! ${IP_NETNS} "${IFCONFIG}" addr add "${host_ip_address_2:?}"/24 broadcast "${broadcast_2:?}" dev ${TAP_2}; then
        echo "Failed to set up IP addressing on ${TAP_2}"
        exit 1
    fi

    if ! ${IP_NETNS} "${IFCONFIG}" link set dev ${TAP_2} up; then
        echo "Failed to bring up ${TAP_2}"
        exit 1
    fi


    if ! ${IP_NETNS} "${IFCONFIG}" route add to "${target_ip_address_2}" dev ${TAP_2}; then
        echo "Failed to add route to ${target_ip_address_2} using ${TAP_2}"
        exit 1
    fi
    echo "Created tap interface: ${TAP_2}"

    # third IP
    if ! ${IP_NETNS} "${IFCONFIG}" addr add "${host_ip_address_3:?}"/16 broadcast "${broadcast_3:?}" dev ${TAP_1}; then
        echo "Failed to set up IP addressing on ${TAP_3}"
        exit 1
    fi

    echo "Add IP to interface: ${TAP_1}"
}

#
# delete_network_interface
#
# Deletes tap-device and interface for the given machine
delete_network_interface() {
    NS_NAME=$1
    if [ -z "$NS_NAME" ]; then
        IP_NETNS="sudo"
    else
        IP_NETNS="sudo ${IFCONFIG} netns exec ${NS_NAME}"
    fi

    ${IP_NETNS} "${IFCONFIG}" link del tap0.73 || true
    ${IP_NETNS} "${IFCONFIG}" tuntap del mode tap tap0 || true
    ${IP_NETNS} "${IFCONFIG}" tuntap del mode tap tap1 || true

    if [ ! -z "$NS_NAME" ]; then
        sudo "${IFCONFIG}" netns del "${NS_NAME}" || true
    fi
}

#
# add necessary configurations to lo interface
# necessary for someipd
#
add_configurations_to_lo_interface() {
  local NS_NAME=$1

  if [ -z "$NS_NAME" ]; then
    local IP_NETNS="sudo"
  else
    local IP_NETNS="sudo ${IFCONFIG} netns exec ${NS_NAME}"
  fi

  if ${IP_NETNS} "${IFCONFIG}" link set lo multicast on; then
    echo "Multicast for loopback interface switched on"
  else
    echo "Failed to switch on multicast for loopback interface"
    exit 1
  fi

  if ${IP_NETNS} "${IFCONFIG}" route add ff01::0/16 dev lo; then
    echo "Route to ff01::0/16 on loopback interface added"
  else
    echo "Could not add route to ff01::0/16 on loopback interface"
    exit 1
  fi
}

#
# remove configurations from lo interface
#
remove_configurations_from_lo_interface() {
  local NS_NAME=$1

  if [ -z "$NS_NAME" ]; then
    local IP_NETNS="sudo"
  else
    local IP_NETNS="sudo ${IFCONFIG} netns exec ${NS_NAME}"
  fi

  # on purpose do not remove multicast from lo
  # as it would have been set beforehand
  # and then we would need further code to handle
  # this case, which is not necessary IMHO

  if ! ${IP_NETNS} ${IFCONFIG} route del ff01::0/16 dev lo; then
    echo "Could not delete route to ff01::0/16 on loopback interface"
  fi
}


script_main () {
    declare target_ip_address_1
    declare target_ip_address_2

    parse_opts "$@"
    check_for_installed_tools
    calculate_network
    if [ -z "${target_ip_address_1+x}" ]; then
        echo "Variable target_ip_address is not set, calculating network information failed."
        exit -1
    fi
    if [ -z "${target_ip_address_2+x}" ]; then
        echo "Variable target_ip_address is not set, calculating network information failed."
        exit -1
    fi
    if [ "${OPT_COMMAND}" == "${CREATE_VALUE}" ];
    then
        create_network_interface "${OPT_NS_NAME}"
        add_configurations_to_lo_interface "${OPT_NS_NAME}"
    else
        remove_configurations_from_lo_interface "${OPT_NS_NAME}"
        delete_network_interface "${OPT_NS_NAME}"
    fi
    return 0
}

script_main "$@"
