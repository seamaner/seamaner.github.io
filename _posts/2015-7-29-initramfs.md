---
layout: blog
title: 简单分析initramfs
---

initramfs 是linux 启动必须的吗？如果kernel有mount root fs所需的所有驱动，initramfs就不是必须的。读取磁盘数据，
kernel需要驱动的帮助，然而物理磁盘各式各样，驱动各不相同，kernel不太可能都包括进来。initramfs是包含少量驱动模块和工具集的archive。
启动时，boot loader将initramfs加载到内存，kernel利用initramfs中的工具加载root fs必须的驱动，进而加载root fs并完成后续的启动过程。

initramfs文件到底是什么内容？
'''
They are simple gzip-compresed cpio archives
'''
本文在Ubuntu环境下，简单分析initramfs文件。



##Find the initrmafs and gunzip

