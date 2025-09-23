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


while getopts i:o:x:b: flag
do
    case "${flag}" in
        i) input=${OPTARG};;
        o) output=${OPTARG};;
        x) executable=${OPTARG};;
        b) backup=${OPTARG};;
    esac
done

corecount=`grep -c ^processor /proc/cpuinfo`
echo "$corecount CPU cores found. Will fuzz on all cores in parallel."

if [ $corecount -eq 0 ]
then
echo "Exiting"
exit 0
fi

echo "Starting $corecount parallel sessions of AFL"

for i in $(seq 1 $corecount)
do
   echo "Started AFL++ on screen afl_screen$i"
   if [ $i -eq 1 ]
   then
       screen -dmS afl_screen$i -- afl-fuzz -M f$i -i $input -o $output $executable
   else
       screen -dmS afl_screen$i -- afl-fuzz -S f$i -i $input -o $output $executable
   fi
done

# Our general exit handler
cleanup() {
    err=$?

    echo ""
    for i in $(seq 1 $corecount)
    do
        echo "Closing screen afl_screen$i"
        screen -S afl_screen$i -X quit
    done

    if [ -d "$backup" ]; then
        echo "Backing up files to $backup ..."
    else
        echo "Creating $backup ..."
        mkdir $backup
    fi
    
    mkdir -p $backup/crashes
    mkdir -p $backup/hangs
    mkdir -p $backup/queues

    for i in $(seq 1 $corecount)
    do
        echo "Backing up $output/f$i"
        cp $output/f$i/crashes/* $backup/crashes
        cp $output/f$i/hangs/* $backup/hangs
        mkdir $backup/queues/f$i
        cp $output/f$i/queue/* $backup/queues/f$i
    done

    trap '' EXIT INT TERM
    exit $err
}

sig_cleanup() {
    trap '' EXIT # some shells will call EXIT after the INT handler
    false # sets $?
    cleanup
}

trap cleanup EXIT
trap sig_cleanup INT QUIT TERM

sleep infinity
