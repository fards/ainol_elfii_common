#!/sbin/sh

/sbin/busybox fdisk /dev/block/avnftl8 < /etc/fdisk.media.cmd
/sbin/busybox fdisk /dev/block/avnftli < /etc/fdisk.media.cmd
