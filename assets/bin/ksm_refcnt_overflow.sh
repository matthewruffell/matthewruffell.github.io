#!/bin/bash

# Script to start and stop KVM virtual machines to try trigger Kernel Samepage
# Mapping zero_page reference counter overflow.
#
# Author: Matthew Ruffell <matthew.ruffell@canonical.com>
# BugLink: https://bugs.launchpad.net/bugs/1837810
#
# Fix: https://github.com/torvalds/linux/commit/7df003c85218b5f5b10a7f6418208f31e813f38f
#
# Instructions:
# ./ksm_refcnt_overflow.sh

# Install QEMU KVM if needed
sudo apt install -y qemu-kvm libvirt-bin qemu-utils genisoimage virtinst

# Enable Kernel Samepage Mapping, use zero_pages
echo 10000 | sudo tee /sys/kernel/mm/ksm/pages_to_scan
echo 1 | sudo tee /sys/kernel/mm/ksm/run
echo 1 | sudo tee /sys/kernel/mm/ksm/use_zero_pages

# Download OS image
wget https://cloud-images.ubuntu.com/xenial/current/xenial-server-cloudimg-amd64-disk1.img
sudo mkdir /var/lib/libvirt/images/base
sudo mv xenial-server-cloudimg-amd64-disk1.img /var/lib/libvirt/images/base/ubuntu-16.04.qcow2

function destroy_all_vms() {
    for i in `sudo virsh list | grep running | awk '{print $2}'`
    do
        virsh shutdown $i &> /dev/null
        virsh destroy $i &> /dev/null
        virsh undefine $i &> /dev/null
        sudo rm -rf /var/lib/libvirt/images/$i
    done
}

function create_single_vm() {
    sudo mkdir /var/lib/libvirt/images/instance-$1
    sudo cp /var/lib/libvirt/images/base/ubuntu-16.04.qcow2 /var/lib/libvirt/images/instance-$1/instance-$1.qcow2
    virt-install --connect qemu:///system \
    --virt-type kvm \
    --name instance-$1 \
    --ram 1024 \
    --vcpus=1 \
    --os-type linux \
    --os-variant ubuntu16.04 \
    --disk path=/var/lib/libvirt/images/instance-$1/instance-$1.qcow2,format=qcow2 \
    --import \
    --network network=default \
    --noautoconsole &> /dev/null
}

function create_destroy_loop() {
    NUM="0"
    while true
    do
        NUM=$[$NUM + 1]
        echo "Run #$NUM"
        for i in {0..7}
        do
            create_single_vm $i
            echo "Created instance $i"
            sleep 10
        done
        sleep 30
        echo "Destroying all VMs"
        destroy_all_vms
    done
}

create_destroy_loop
