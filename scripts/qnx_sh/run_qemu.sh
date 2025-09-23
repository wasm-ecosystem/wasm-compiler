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

sudo /usr/bin/qemu-system-x86_64 -nodefaults -nographic -vga none -machine q35 -cpu host -smp 2,maxcpus=2,cores=2 -m 4G -accel kvm -pflash ./downloaded -object rng-random,filename=/dev/random,id=rng0 -device virtio-rng-pci,rng=rng0 -drive file=./disk.img,format=qcow2,if=ide,id=disk0,file.locking=off -drive file=./disk.img,format=qcow2,if=virtio,file.locking=off  -netdev tap,id=t1,ifname=tap0,script=no,downscript=no -device virtio-net-pci,ioeventfd=off,netdev=t1,id=nic1,mac=52:54:11:22:33:01  -netdev tap,id=t2,ifname=tap1,script=no,downscript=no -device virtio-net-pci,ioeventfd=off,netdev=t2,id=nic2,mac=52:54:11:22:33:02 -serial mon:stdio
