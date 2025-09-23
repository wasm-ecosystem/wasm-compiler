#!/bin/sh
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



aws_instance_id=`curl -s http://169.254.169.254/latest/meta-data/instance-id`
backup_folder="/efs/$aws_instance_id"

echo "Creating directory $backup_folder ..."
if ! mkdir -p $backup_folder ; then
    echo "Failed to create directory $backup_folder"
fi

log() {
    echo $1
    echo "$(date): $1" >> $backup_folder/log.txt
}
log "AWS Instance ID: $aws_instance_id"

corecount=`grep -c ^processor /proc/cpuinfo`
log "--- INIT"
log "$corecount CPU cores found. Will fuzz on all cores in parallel."

if [ $corecount -eq 0 ]
then
   echo "Exiting"
   exit 0
fi

for i in $(seq 1 $corecount)
do
   if mkdir -p /mnt/ramdisk/$i ; then
      echo "Starting Fuzzer on screen fuzz_screen$i"
      screen -dmS fuzz_screen$i -- docker run --rm -e TRICORE_QEMU_PATH=/host$TRICORE_QEMU_PATH -e TRICORE_GDB_PATH=/host$TRICORE_GDB_PATH -e VB_FUZZ_OFFSET=$i -e VB_FUZZ_TARGET_DIR="/ramdisk/$i" -e VB_FUZZ_EXEC_PREFIX="/host/usr/local/bin/" -e VB_FUZZ_NATIVE_BUILD_FOLDER="/host/home/ubuntu/vb/native_build" -v /:/host:ro -v /mnt/ramdisk:/ramdisk:rw --name fuzz_container$i fuzzer-image bash -c "echo \$VB_FUZZ_TARGET_DIR && cd /host/home/ubuntu/ && ./gdb_tc_qemu_fuzz_relative.sh ./vb/build/bin/vb_gdb_fuzz"
      rm -f /mnt/ramdisk/$i/status.txt
   else
      echo "Failed to create directory /mnt/ramdisk/$i"
   fi
done

# Our general exit handler
cleanup() {
    err=$?

    echo ""

    log "Cleaning up ..."
    log "Backing up failed modules"

    if mkdir -p $backup_folder/failedmodules ; then
        for i in $(seq 1 $corecount); do
            echo "Backing up /mnt/ramdisk/$i"
            if [ -d "/mnt/ramdisk/$i/failedmodules" ]; then
                cp --backup=numbered /mnt/ramdisk/$i/failedmodules/*.wasm $backup_folder/failedmodules
            fi
        done
    else
	    log "Failed to create directory $backup_folder/failedmodules"
    fi

    for i in $(seq 1 $corecount); do
        echo "Killing container fuzz_container$i"
        docker kill --signal=SIGKILL fuzz_container$i
        echo "Closing screen fuzz_screen$i"
        screen -S fuzz_screen$i -X quit
    done

    trap '' EXIT INT TERM
    exit $err
}

sig_cleanup() {
    trap '' EXIT # some shells will call EXIT after the INT handler
    false # sets $?
    log "Exiting due to signal ..."
    cleanup
}

trap cleanup EXIT
trap sig_cleanup INT QUIT TERM

last_instances_active=0
AWS_AUTH_TOKEN=`curl -s -X PUT "http://169.254.169.254/latest/api/token" -H "X-aws-ec2-metadata-token-ttl-seconds: 21600"`
while true; do
    HTTP_CODE=$(curl -H "X-aws-ec2-metadata-token: $AWS_AUTH_TOKEN" -s -w %{http_code} -o /dev/null http://169.254.169.254/latest/meta-data/spot/instance-action)
    if [ "$HTTP_CODE" -eq 401 ]; then
        echo 'Refreshing Authentication Token'
        AWS_AUTH_TOKEN=`curl -s -X PUT "http://169.254.169.254/latest/api/token" -H "X-aws-ec2-metadata-token-ttl-seconds: 30"`
    elif [ "$HTTP_CODE" -eq 200 ]; then
        # Insert Your Code to Handle Interruption Here
        log 'AWS Spot Instance Interruption Detected'
        log 'Cleaning up ...'
        cleanup
    else
        echo 'AWS Spot Not Interrupted'
    fi

    total_calls=0
    total_modules=0
    total_failed=0
    total_fps_last_modules=0
    total_fps_all=0
	last_seconds=0

	instances_active=0
    running_containers=$(docker ps --format '{{.Names}}')
    for i in $(seq 1 $corecount); do
        if $(echo "$running_containers" | grep -q "^fuzz_container$i$"); then
            instances_active=$((instances_active + 1))
            # else
            # log "fuzz_container$i crashed"
            # log "fuzz_container$1 crashed, backing up current Wasm and restarting ..."
            # if mkdir -p $backup_folder/failedmodules ; then
            #     for i in $(seq 1 $corecount); do
            #         log "Backing up /mnt/ramdisk/$i/new.wasm"
            #         cp --backup=numbered /mnt/ramdisk/$i/new.wasm $backup_folder/failedmodules/crash.wasm
            #     done
            # else
            #     log "Failed to create directory $backup_folder/failedmodules"
            # fi
            # log "Restarting fuzz_container$1"
            # echo "Starting Fuzzer on screen fuzz_screen$i"
            # screen -dmS fuzz_screen$i -- docker run --rm -e TRICORE_QEMU_PATH=/host$TRICORE_QEMU_PATH -e TRICORE_GDB_PATH=/host$TRICORE_GDB_PATH -e VB_FUZZ_OFFSET=$i -e VB_FUZZ_TARGET_DIR="/ramdisk/$i" -e VB_FUZZ_EXEC_PREFIX="/host/usr/local/bin/" -v /:/host:ro -v /mnt/ramdisk:/ramdisk:rw --name fuzz_container$i fuzzer-image bash -c "echo \$VB_FUZZ_TARGET_DIR && cd /host/home/ubuntu/ && ./gdb_tc_qemu_fuzz_relative.sh ./vb/build/bin/vb_gdb_fuzz"
            # rm -f /mnt/ramdisk/$i/status.txt
        else
            continue
        fi
        if [ -f "/mnt/ramdisk/$i/status.txt" ]; then
            status=$(cat "/mnt/ramdisk/$i/status.txt")

            calls=$(echo "$status" | grep -oE '[0-9]+ function calls' | grep -oE '[0-9]+')
            modules=$(echo "$status" | grep -oE '[0-9]+ modules\) executed' | grep -oE '[0-9]+')
            seconds=$(echo "$status" | grep -oE 'executed in [0-9\.]+' | grep -oE '[0-9\.]+')
            failed=$(echo "$status" | grep -oE '[0-9]+ failed' | grep -oE '[0-9]+')
            fps=$(echo "$status" | grep -oE '[0-9\.]+ f/s \(last' | grep -oE '[0-9\.]+')
            fps_all=$(echo "$status" | grep -oE '[0-9\.]+ f/s \(all' | grep -oE '[0-9\.]+')

            last_seconds=$seconds
            total_calls=$((total_calls + calls))
            total_modules=$((total_modules + modules))
            total_failed=$((total_failed + failed))
            total_fps_last_modules=$(echo "$total_fps_last_modules + $fps" | bc)
            total_fps_all=$(echo "$total_fps_all + $fps_all" | bc)
        fi
    done
	last_modules=$((10 * corecount))
	instances_active_log="$instances_active/$corecount instances active"
    if [ "$last_instances_active" != "$instances_active" ]; then
        echo "$(date): $instances_active/$corecount instances active" >> $backup_folder/log.txt
    fi
    last_instances_active=$instances_active
	other_log="$total_calls function calls ($total_modules modules) executed in $last_seconds s ($total_failed failed) - $total_fps_last_modules f/s (last $last_modules modules), $total_fps_all f/s (all)"
    echo "$instances_active_log"
    echo "$other_log"
    echo "$(date)\n$instances_active_log\n$other_log" > $backup_folder/status.txt
    sleep 1
done
