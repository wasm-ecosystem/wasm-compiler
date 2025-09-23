#!/usr/bin/python
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

# vim:tw=100:sw=4:ts=4:sts=4:et
# pylint: disable=invalid-name,missing-docstring,too-few-public-methods

import json, sys, os, logging, argparse


class MachineConfig:
    def __init__(self, ecu=None):
        logging.basicConfig()
        self.__logger = logging.getLogger("MachineConfig")

        self.__MACHINE_SPECIFIC_CONFIG_FILE = "machine_specific_config.json"

        self.__k_ecu_configs = ["m_pp", "m_high_pp"]

        self.__k_host_ip_address_1 = "host_ip_address_1"
        self.__k_target_ip_address_1 = "target_ip_address_1"
        self.__k_broadcast_1 = "broadcast_1"
        self.__k_host_ip_address_2 = "host_ip_address_2"
        self.__k_target_ip_address_2 = "target_ip_address_2"
        self.__k_broadcast_2 = "broadcast_2"
        self.__k_host_ip_address_3 = "host_ip_address_3"
        self.__k_target_ip_address_3 = "target_ip_address_3"
        self.__k_broadcast_3 = "broadcast_3"

        self.__v_host_ip_address_1 = None
        self.__v_target_ip_address_1 = None
        self.__v_broadcast_1 = None
        self.__v_host_ip_address_2 = None
        self.__v_target_ip_address_2 = None
        self.__v_broadcast_2 = None
        self.__v_host_ip_address_3 = None
        self.__v_target_ip_address_3 = None
        self.__v_broadcast_3 = None

        if not os.path.isfile(self.__MACHINE_SPECIFIC_CONFIG_FILE):
            self.__logger.debug("Looking for configuratrion file next to python script")
            self.__MACHINE_SPECIFIC_CONFIG_FILE = os.path.join(
                os.path.dirname(os.path.realpath(__file__)),
                self.__MACHINE_SPECIFIC_CONFIG_FILE,
            )

        if not os.path.isfile(self.__MACHINE_SPECIFIC_CONFIG_FILE):
            self.__logger.error(
                "Cannot find configuratrion file : "
                + self.__MACHINE_SPECIFIC_CONFIG_FILE
            )
            sys.exit()
        elif ecu is None:
            self.__logger.error("No ECU configuration is specified")
            sys.exit()
        elif ecu not in self.__k_ecu_configs:
            self.__logger.error("Invalid ECU configuration : " + str(ecu))
            sys.exit()
        else:
            f = open(self.__MACHINE_SPECIFIC_CONFIG_FILE)
            try:
                data = json.load(f)
                self.__v_host_ip_address_1 = data[ecu][self.__k_host_ip_address_1]
                self.__v_target_ip_address_1 = data[ecu][self.__k_target_ip_address_1]
                self.__v_broadcast_1 = data[ecu][self.__k_broadcast_1]
                self.__v_host_ip_address_2 = data[ecu][self.__k_host_ip_address_2]
                self.__v_target_ip_address_2 = data[ecu][self.__k_target_ip_address_2]
                self.__v_broadcast_2 = data[ecu][self.__k_broadcast_2]
                self.__v_host_ip_address_3 = data[ecu][self.__k_host_ip_address_3]
                self.__v_target_ip_address_3 = data[ecu][self.__k_target_ip_address_3]
                self.__v_broadcast_3 = data[ecu][self.__k_broadcast_3]
            except ValueError:
                self.__logger.error(
                    "Decoding JSON has failed : "
                    + str(self.__MACHINE_SPECIFIC_CONFIG_FILE)
                )
                sys.exit()
            self.validate_addresses()

    def validate_ip(self, s):
        a = s.split(".")
        if len(a) != 4:
            return False
        for x in a:
            if not x.isdigit():
                return False
            i = int(x)
            if i < 0 or i > 255:
                return False
        return True

    def validate_addresses(self):
        for address in (
            self.__v_host_ip_address_1,
            self.__v_target_ip_address_1,
            self.__v_broadcast_1,
            self.__v_host_ip_address_2,
            self.__v_target_ip_address_2,
            self.__v_broadcast_2,
            self.__v_host_ip_address_3,
            self.__v_target_ip_address_3,
            self.__v_broadcast_3,
        ):
            if not self.validate_ip(address):
                self.__logger.error("Invalid IP address : " + str(address))
                sys.exit()
                break

    @property
    def host_ip_address_1(self):
        return self.__v_host_ip_address_1

    @property
    def target_ip_address_1(self):
        return self.__v_target_ip_address_1

    @property
    def broadcast_1(self):
        return self.__v_broadcast_1

    @property
    def host_ip_address_2(self):
        return self.__v_host_ip_address_2

    @property
    def target_ip_address_2(self):
        return self.__v_target_ip_address_2

    @property
    def broadcast_2(self):
        return self.__v_broadcast_2

    @property
    def host_ip_address_3(self):
        return self.__v_host_ip_address_3

    @property
    def target_ip_address_3(self):
        return self.__v_target_ip_address_3

    @property
    def broadcast_3(self):
        return self.__v_broadcast_3

    @property
    def configuration(self):
        return (
            self.__v_host_ip_address_1,
            self.__v_target_ip_address_1,
            self.__v_broadcast_1,
            self.__v_host_ip_address_2,
            self.__v_target_ip_address_2,
            self.__v_broadcast_2,
            self.__v_host_ip_address_3,
            self.__v_target_ip_address_3,
            self.__v_broadcast_3,
        )


def main():
    parser = argparse.ArgumentParser(
        "This is a help for flash sequence automation script\n"
    )
    parser.add_argument(
        "-t",
        "--target",
        type=str,
        nargs=1,
        metavar="target",
        default=["m_pp"],
        help="Specifies the target ECU. Possible values are m_pp and m_high_pp",
    )
    args = parser.parse_args()
    if args.target is not None:
        config = MachineConfig(args.target[0])
    output = ""
    for address in config.configuration:
        output = output + " " + address
    print(output)


if __name__ == "__main__":
    main()
