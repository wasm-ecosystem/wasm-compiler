#!/usr/bin/env python3
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


import subprocess
import time
import sys
import argparse
import logging
import os


def setup_logging(debug=False):
    """Set up logging configuration."""
    level = logging.DEBUG if debug else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(asctime)s - %(levelname)s - %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )


def launch_emulator(avd_name="x86_64_emulator"):
    """Launch the Android emulator"""
    logging.info("Starting Android emulator...")
    emulator_cmd = [
        "/opt/android-sdk/emulator/emulator",
        "-avd",
        avd_name,
        "-no-audio",
        "-no-boot-anim",
        "-partition-size",
        "2048",
        "-gpu",
        "swiftshader_indirect",
        "-no-window",
    ]

    emulator_process = subprocess.Popen(
        emulator_cmd, stdout=sys.stdout, stderr=sys.stderr, env=os.environ
    )

    return emulator_process


def is_emulator_ready(timeout=180, check_interval=5):
    """Check if the emulator is running and ready."""
    logging.info("Checking if emulator is ready...")

    # First wait for device to be detected
    try:
        subprocess.run(
            ["/opt/android-sdk/platform-tools/adb", "wait-for-device"],
            check=True,
            timeout=timeout,
        )
    except subprocess.SubprocessError as e:
        logging.error(f"Failed while waiting for device: {e}")
        return False

    # Then wait for boot to complete
    end_time = time.time() + timeout
    while time.time() < end_time:
        try:
            result = subprocess.run(
                [
                    "/opt/android-sdk/platform-tools/adb",
                    "shell",
                    "getprop",
                    "sys.boot_completed",
                ],
                capture_output=True,
                text=True,
                check=False,
                timeout=10,
            )

            # Check if boot completed
            if result.stdout.strip() == "1":
                return True

        except subprocess.SubprocessError as e:
            logging.debug(f"Error checking emulator status: {e}")

        logging.info("Emulator not ready yet, waiting...")
        time.sleep(check_interval)

    logging.error("Timed out waiting for emulator to boot")
    return False


def run_tests():
    """Run the spectest on the emulator."""
    try:
        logging.info("Pushing test binary to the emulator...")
        subprocess.run(
            [
                "/opt/android-sdk/platform-tools/adb",
                "push",
                "./build_android/bin/vb_spectest_json",
                "/tmp",
            ],
            check=True,
        )

        logging.info("Pushing test cases to the emulator...")
        subprocess.run(
            [
                "/opt/android-sdk/platform-tools/adb",
                "push",
                "./tests/testcases.json",
                "/tmp",
            ],
            check=True,
        )

        # Make the binary executable
        subprocess.run(
            [
                "/opt/android-sdk/platform-tools/adb",
                "shell",
                "chmod +x /tmp/vb_spectest_json",
            ],
            check=True,
        )

        logging.info("Running tests on the emulator...")
        process = subprocess.Popen(
            [
                "/opt/android-sdk/platform-tools/adb",
                "shell",
                "/tmp/vb_spectest_json",
                "/tmp/testcases.json",
            ],
            stdout=sys.stdout,
            stderr=sys.stderr,
        )

        # Wait for the test to complete and get the return code
        return_code = process.wait()
        return return_code
    except subprocess.SubprocessError as e:
        logging.error(f"Test execution failed: {e}")
        return 1


def parse_args():
    parser = argparse.ArgumentParser(description="Run Android tests in emulator")
    parser.add_argument(
        "--avd", default="x86_64_emulator", help="Name of the AVD to use"
    )
    parser.add_argument(
        "--timeout", type=int, default=180, help="Timeout for emulator boot in seconds"
    )
    parser.add_argument("--debug", action="store_true", help="Enable debug logging")
    return parser.parse_args()


def main():
    args = parse_args()
    setup_logging(args.debug)

    # Launch the emulator
    emulator_process = launch_emulator(args.avd)

    # Check if emulator is ready
    if not is_emulator_ready(timeout=args.timeout):
        sys.exit(1)

    try:
        # Run the tests
        exit_code = run_tests()
        print(f"Test completed with exit code: {exit_code}")
    finally:
        # Kill the emulator before exiting
        logging.info("Shutting down the emulator...")
        subprocess.run(
            ["/opt/android-sdk/platform-tools/adb", "emu", "kill"], check=False
        )

    # Return test result
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
