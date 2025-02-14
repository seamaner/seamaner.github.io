---
layout: post
title: 内核内存管理-虚拟地址到物理地址转换实验
categories: kernel
description: 从虚机地址到物理地址转换过程
keywords: kernel, 内核
---

虚拟是如何一步步转为物理地址的？用户态程序运行时，访问内存的指令，cpu在运行时会根据CR3寄存器的值一步步查询到最终的物理地址，这一过程是由MMU自动完成的。虚拟地址到物理地址到底是如何转换的？借助qemu我们可以通过实验查看整个转换过程是如何工作的。  

## 虚拟地址  

可以从/proc/self/maps查看到展示用户空间进程的各个内存区:  
```
561b0feb4000-561b0feb5000 r--p 00000000 fd:00 917628                     /home/ubuntu/kernel/hack/mm
561b0feb5000-561b0feb6000 r-xp 00001000 fd:00 917628                     /home/ubuntu/kernel/hack/mm
561b0feb6000-561b0feb7000 r--p 00002000 fd:00 917628                     /home/ubuntu/kernel/hack/mm
561b0feb7000-561b0feb8000 r--p 00002000 fd:00 917628                     /home/ubuntu/kernel/hack/mm
561b0feb8000-561b0feb9000 rw-p 00003000 fd:00 917628                     /home/ubuntu/kernel/hack/mm
561b2be8b000-561b2beac000 rw-p 00000000 00:00 0                          [heap]
7f1b6d3d4000-7f1b6d3d7000 rw-p 00000000 00:00 0
7f1b6d3d7000-7f1b6d3ff000 r--p 00000000 fd:00 137558                     /usr/lib/x86_64-linux-gnu/libc.so.6
7f1b6d3ff000-7f1b6d594000 r-xp 00028000 fd:00 137558                     /usr/lib/x86_64-linux-gnu/libc.so.6
7f1b6d594000-7f1b6d5ec000 r--p 001bd000 fd:00 137558                     /usr/lib/x86_64-linux-gnu/libc.so.6
7f1b6d5ec000-7f1b6d5ed000 ---p 00215000 fd:00 137558                     /usr/lib/x86_64-linux-gnu/libc.so.6
7f1b6d5ed000-7f1b6d5f1000 r--p 00215000 fd:00 137558                     /usr/lib/x86_64-linux-gnu/libc.so.6
7f1b6d5f1000-7f1b6d5f3000 rw-p 00219000 fd:00 137558                     /usr/lib/x86_64-linux-gnu/libc.so.6
7f1b6d5f3000-7f1b6d600000 rw-p 00000000 00:00 0
7f1b6d60a000-7f1b6d60c000 rw-p 00000000 00:00 0
7f1b6d60c000-7f1b6d60e000 r--p 00000000 fd:00 132206                     /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
7f1b6d60e000-7f1b6d638000 r-xp 00002000 fd:00 132206                     /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
7f1b6d638000-7f1b6d643000 r--p 0002c000 fd:00 132206                     /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
7f1b6d644000-7f1b6d646000 r--p 00037000 fd:00 132206                     /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
7f1b6d646000-7f1b6d648000 rw-p 00039000 fd:00 132206                     /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
7ffcae07f000-7ffcae0a0000 rw-p 00000000 00:00 0                          [stack]
7ffcae11c000-7ffcae120000 r--p 00000000 00:00 0                          [vvar]
7ffcae120000-7ffcae122000 r-xp 00000000 00:00 0                          [vdso]
ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0                  [vsyscall]
```

观察各个内存区，在范围上他们都小于0x7ffffffff000, 实际上，在x64上地址区间是这样分布的：  
-  128TB以下用户空间，64位地址的低48位，48 = 4 * 9 + 12 ， paging的过程
-  128以上线性地址， 内核kmalloc， object pool
-  128以上vmalloc， 这部分地址管理上和用户空间地址是相似的，都是虚拟地址，区别在于使用的PGT不太，vmalloc使用的内核pgd，即`init_mm.pgd`.   

```
(gdb) p init_mm.pgd
$3 = (pgd_t *) 0xffffffff84c26000 <init_top_pgt>
(gdb)
```

mm-paging example, 打印出malloc分配的内存地址，也就是虚拟地址：  

```
 cat hello-mm.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
  char *p = malloc(128);
  if (p) {
      strcpy(p, "Hello mm paging!\n");
      printf("p(%p) is: %s\n", p, p);
      printf("sleep here!!!!\n");
      sleep(1000000);
  } else {
      printf("Failed to alloc 128 chars\n");
  }
  return 0;
}
```   

首先通过qemu启动一个虚拟机，并在其中运行example程序，打印出虚拟地址：  

```
#
./mm-paging
p(0x5614df8812a0) is: Hello mm paging!

sleep here!!!!
```
从虚拟地址可以得到各个page table地址：  
0x5614df8812a0从第13bit起到第48bit的36位是4级页表的"地址", 每9位构成一个当前页表中的地址，是一个0~511的数，也就是当前level页表中的offset下标。  
对0x5614df8812a0来说，四级页表地址分别是：  
- 0x5614df8812a0 >> (12+27) & (2**9 - 1) -> 0xac
- 0x5614df8812a0 >> (12+18) & (2**9 - 1) -> 0x53
- 0x5614df8812a0 >> (12+9)  & (2**9 - 1) -> 0xfc
- 0x5614df8812a0 >> (12)    & (2**9 - 1) -> 0x81
   
页中的偏移地址为：  
`0x5614df8812a0 & 0xfff -> 0x2a0`
通过gdb qemu 调试虚机，查看exaple程序(mm-paging)的页表信息：
```
gdb ./vmlinux
target remote:1234
```

#mapping过程  

从task->mm->pgd拿到mm-paging进程的PGD地址：   
```

>lx-ps
      TASK          PID    COMM
0xffff888006804ec0  214  mm-paging

p ((struct task_struct*)0xffff888006804ec0)->mm->pgd
$1 = (pgd_t *) 0xffff8880068c8000
```

内核地址都在0xffff800000000000以上部分，内核内访问物理地址是通过 物理地址+ page_offset_base 的**线性地址**访问的:    

```
x/a &page_offset_base
0xffffffff8323f1f8 <page_offset_base>:  0xffff888000000000
```

现在有了PGD，也有了page_offset_base可以访问任意物理地址，也有了各级页表的offset(0xac, 0x53, 0xfc, 0x81), 现在可以遍历拿到虚拟地址真正的物理地址了:  

```

- L4:
 x/gx (0xffff8880068c8000 + 0xac * 8)
0xffff8880068c8560:     0x8000000005847067

- L3:
x/gx (0x5847000 + 0x53 * 8 + 0xffff888000000000)
0xffff888005847298:     0x00000000078c4067

- L2:
x/gx (0x78c4000 + 0xfc *8 + 0xffff888000000000)
0xffff8880078c47e0:     0x0000000005bb3067

- L1:
x/gx(0x5bb3000 + 0x81 * 8 + 0xffff888000000000)
0xffff888005bb3408:     0x8000000006565067
```

check the memory: page + offset:  

```
x/s (0x5614df8812a0 & 0xfff) + (0x6565000 + 0xffff888000000000)
0xffff8880065652a0:     "Hello mm paging!\n"
```

直接访问物理地址打印出的字符串，正是我们设置的内容，这说明这个"寻址"过程是正确的。  

