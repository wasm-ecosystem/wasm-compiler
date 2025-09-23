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

import paramiko, subprocess, time, argparse
from scp import SCPClient


parser = argparse.ArgumentParser(description="A script with a command-line argument.")
parser.add_argument("--arch", type=str, help="select qemu, x64 or arm64")
args = parser.parse_args()

if args.arch:
    if args.arch != "x64" and args.arch != "arm64":
        print("arch not right ,select x64 or arm64.")
        exit(1)
else:
    print("no arch selected. x64 or arm64")
    exit(1)

if args.arch == "x64":
    cmd = ["./run_qemu.sh"]
    target_ip = "169.254.21.103"
    vb_spectest_json_path = "../../build_qnx/bin/vb_spectest_json"
    testcases_json_path = "../../tests/testcases.json"
else:
    cmd = ["./run.sh"]
    target_ip = "160.48.199.101"
    vb_spectest_json_path = "../build_qnx/bin/vb_spectest_json"
    testcases_json_path = "../tests/testcases.json"

proc = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE)
# create SSH client
ssh = paramiko.SSHClient()

# add host key policy
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

# connect to remote server
for i in range(0, 120):
    try:
        ssh.connect(
            target_ip,
            username="root",
            password="",
            allow_agent=False,
            look_for_keys=False,
            timeout=120,
        )
        # Set up keep-alive mechanism (sends a packet every 60 seconds)
        transport = ssh.get_transport()
        transport.set_keepalive(10)
        break  # Connection successful, exit the retry loop
    except Exception as e:
        print(f"SSH connect failed: {str(e)}, retry attempt {i+1}/120")
        time.sleep(1)
        if i == 119:
            print("Exceeded maximum attempts. Exiting the program.")
            exit(1)

print("ssh connected.")
# Transfer file
local_file1 = vb_spectest_json_path
local_file2 = testcases_json_path
remote_file_path1 = "/tmp/vb_spectest_json"
remote_file_path2 = "/tmp/testcases.json"

scp = SCPClient(ssh.get_transport())

scp.put(local_file1, remote_file_path1)
scp.put(local_file2, remote_file_path2)


# execute command
print("exec spectest")
# Use get_pty=True to get a pseudo-terminal which helps with streaming
stdin, stdout, stderr = ssh.exec_command(
    "/tmp/vb_spectest_json /tmp/testcases.json", get_pty=True
)

# Stream the output in real-time
print("Command Output:")
while not stdout.channel.exit_status_ready():
    # Check and read stdout
    if stdout.channel.recv_ready():
        output = stdout.channel.recv(1024).decode("utf-8")
        print(output, end="", flush=True)

    # Check and read stderr
    if stderr.channel.recv_stderr_ready():
        error_output = stderr.channel.recv_stderr(1024).decode("utf-8")
        print(error_output, end="", flush=True)

    # Small delay to prevent high CPU usage
    time.sleep(0.1)

# Get any remaining output from both stdout and stderr
if stdout.channel.recv_ready():
    output = stdout.channel.recv(1024).decode("utf-8")
    print(output, end="", flush=True)

if stderr.channel.recv_stderr_ready():
    error_output = stderr.channel.recv_stderr(1024).decode("utf-8")
    print(error_output, end="", flush=True)

# Get the return code
return_code = stdout.channel.recv_exit_status()
print("Spectest Return Code:", return_code)

stdin.close()
stdout.close()
stderr.close()

# close SSH connection
ssh.close()

# Send the Ctrl + A key combination
proc.stdin.write(b"\x01")
# Send X
proc.stdin.write(b"X")

try:
    proc.wait(timeout=5)
    proc.kill()
except Exception:
    pass

exit(return_code)
