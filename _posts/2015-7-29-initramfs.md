---
layout: blog
title: 简单分析initramfs
---

initramfs 是linux 启动必须的吗？如果kernel有mount root fs所需的所有驱动，initramfs就不是必须的。读取磁盘数据，
kernel需要驱动的帮助，然而物理磁盘各式各样，驱动各不相同，kernel不太可能都包括进来。initramfs是包含少量驱动模块和工具集的archive。
启动时，boot loader将initramfs加载到内存，kernel利用initramfs中的工具加载root fs必须的驱动，进而加载root fs并完成后续的启动过程。

initramfs文件到底是什么内容？

```
They are simple gzip-compresed cpio archives
```

本文在Ubuntu环境下，简单分析initramfs文件。



##Find the initrmafs and gunzip
```
~$ mkdir initfs
~$ cd initfs/
~/initfs$ cp /boot/initrd.img-3.13.0-24-generic .

~/initfs$ file initrd.img-3.13.0-24-generic 
initrd.img-3.13.0-24-generic: gzip compressed data, from Unix, last modified: Fri Jul 18 22:56:46 2014
~/initfs$ mv initrd.img-3.13.0-24-generic initrd.img.gz
~/initfs$ gzip -d initrd.img.gz 
```
需要将initrd.img-3.13.0-24-generic添加.gz后缀名，才能解压，gzip检查后缀名，这在linux里倒是比较少见的。

## cpio
```
$ file initrd.img 
initrd.img: ASCII cpio archive (SVR4 with no CRC)
$ cpio -i < initrd.img 
82506 blocks
$ ls
bin  conf  etc  init  initrd.img  lib  run  sbin  scripts
###bin
bin:
.   busybox  date  dmesg   halt    ipconfig  kmod      losetup  nfsmount  pivot_root  reboot  run-init  sh     udevadm
..  cpio     dd    fstype  insmod  kbd_mode  loadkeys  mount    ntfs-3g   poweroff    resume  setfont   sleep
init ---- init脚本
sbin:
blkid  dmsetup  dumpe2fs  hwclock  modprobe  mount.fuse  mount.ntfs  mount.ntfs-3g  rmmod  udevadm  wait-for-root
```
