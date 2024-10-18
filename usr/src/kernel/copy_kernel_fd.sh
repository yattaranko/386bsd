#!/bin/sh

echo "mount /dev/fd1 /mnt"
mount /dev/fd1 /mnt
echo "copy obj/386bsd /mnt"
cp obj/386bsd /mnt
echo "unmount /mnt"
umount /mnt

