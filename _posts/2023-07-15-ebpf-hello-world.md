---
layout: post
title: eBPF：hello world
categories: eBPF
description: 用eBPF字节码写的“hello world”
keywords: eBPF
---

有许多用libbpf或者go-bpf写的eBPF例子，python等别的语言的也有很多。这些都不够直观，代码都经过了层层包装，不能很好的明白eBPF是怎么工作的。
本文用最原始最直接的方式展示eBPF是如何工作的---用eBPF字节码写一个“hello world”。

## 什么是eBPF

"register-based virtual machine" 基于寄存器的虚拟机。不同于KVM等system virtual machine技术，和Java 虚拟机、Lua虚拟机类似，是一种process virtual machine。
进程虚拟机最常用的设计模式有两种：
- register-base virtual machine 
- stack-based virtual machine
  
两种设计模式没有本质的区别，只是计算方式不同。 
计算两个整数相加，用register-based virtual machine，字节码是： 
```
 1 LOAD reg0 #100
 2 LOAD reg1 #200
 3 ADD reg2 reg1 reg0
```
第1、2行把100、200分别存入寄存器reg0和reg1；  
第3行把两个数相加，把结果存到寄存器reg2上。  
如果用基于stack的方式计算，数据先放在stack上的，计算时先执行两次POP，计算结果再存回到栈上。  
```
r = STACK[sp--];
l = STACK[sp--];
STACK[++sp] = (r + l);
``` 
eBPF也可以说是跑在内核空间内，只能运行内核允许运行的任务的语言，是一种“特定领域语言”。如果不考虑JIT，只是模拟运行，几乎可以随意设计“特定领域语言”，一个简单的register-based vm：

```
#include "stdio.h"
 
#define NUM_REGS 4
#define TRUE    1
#define FALSE   0
#define INVALID -1
  
enum opCodes {
    HALT  = 0x0,
    LOAD  = 0x1,
    ADD   = 0x2,
};
 
/*
 * Register set of the VM
 */
int regs[NUM_REGS];
 
/*
 * VM specific data for an instruction
 */
struct VMData_ {
  int reg0;
  int reg1;
  int reg2;
  int reg3;
  int op;
  int scal;
};
typedef struct VMData_ VMData;
 
int  fetch();
void decode(int instruction, VMData *data);
void execute(VMData *data);
void run();
 
/*
 * Addressing Modes:
 * - Registers used as r0, r1,..rn.
 * - Scalar/ Constant (immediate) values represented as #123
 * - Memory addresses begin with @4556
 */
 
/*
 * Instruction set:
 * - Load an immediate number (a constant) into a register
 * - Perform an arithmetic sum of two registers (in effect,
 *   adding two numbers)
 * - Halt the machine
 *
 * LOAD reg0 #100
 * LOAD reg1 #200
 * ADD reg2 reg1 reg0  // 'reg2' is destination register
 * HALT
 */
 
/*
 * Instruction codes:
 * Since we have very small number of instructions, we can have
 * instructions that have following structure:
 * - 16-bit instructions
 *
 * Operands get 8-bits, so range of number supported by our VM
 * will be 0-255.
 * The operands gets place from LSB bit position
 * |7|6|5|4|3|2|1|0|
 *
 * Register number can we encoded in 4-bits 
 * |11|10|9|8|
 *
 * Rest 4-bits will be used by opcode encoding.
 * |15|14|13|12|
 *
 * So an "LOAD reg0 #20" instruction would assume following encoding:
 * <0001> <0000> <00010100>
 * or 0x1014 is the hex representation of given instruction.
 */
 
 
 /*
  * Sample program with an instruction set
  */
 
unsigned int code[] = {0x1014,
                       0x110A,
                       0x2201,
                       0x0000};
 
/*
 * Instruction cycle: Fetch, Decode, Execute
 */
 
/*
 * Current state of machine: It's a binary true/false
 */
 
int running = TRUE;
 
/*
 * Fetch
 */
int fetch()
{
  /*
   * Program Counter
   */
  static int pc = 0;
 
  if (pc == NUM_REGS)
    return INVALID;
 
  return code[pc++];
}
 
void decode(int instr, VMData *t)
{
  t->op   = (instr & 0xF000) >> 12;
  t->reg1 = (instr & 0x0F00) >> 8;
  t->reg2 = (instr & 0x00F0) >> 4;
  t->reg3 = (instr & 0x000F);
  t->scal = (instr & 0x00FF);
}
 
void execute(VMData *t)
{
  switch(t->op) {
    case 1:
      /* LOAD */
      printf("\nLOAD REG%d %d\n", t->reg1, t->scal);
      regs[t->reg1] = t->scal;
      break;
 
    case 2:
      /* ADD */
      printf("\nADD %d %d\n", regs[t->reg2], regs[t->reg3]);
      regs[t->reg1] = regs[t->reg2] + regs[t->reg3];
      printf("\nResult: %d\n", regs[t->reg1]);
      break;
 
    case 0:
    default:
      /* Halt the machine */
      printf("\nHalt!\n");
      running = FALSE;
      break;
    }
}
 
void run()
{
  int instr;
  VMData t;
 
  while(running)
  {
    instr = fetch();
 
    if (INVALID == instr)
      break;
 
    decode(instr, &t);
    execute(&t);
  }
}
 
int main()
{
  run();
  return 0;
}
```

那么eBPF是如何设计的？看一下eBPF字节码。

## eBPF字节码

`opcode:8 src_reg:4 dst_reg:4 offset:16 imm:32 // In little-endian BPF.` 

## hello world

C hello world
就是把“hello world”输出出来。
BPF系统调用
/* 补充 man bpf内容*/
打印函数
如何把字符串传给给内核呢？
字符串共13个字节。可以分拆成几段，这样可以存在寄存器上，接着可以保存到stack上。
最后，调用print函数完成输出。

代码最终的结果：

```
struct bpf_insn bpf_prog[] = {
    { 0xb7, 1, 0, 0, 0 }, //r1 = 0
    { 0x73, 0xa, 0x1, 0xfffc, 0}, //*(u8*)(r10 - 4 ) = r1
    { 0xb7, 1, 0, 0, 0x0a646c72}, //r1 = b'\ndlr'
    { 0x63, 0xa, 0x1, 0xfff8, 0}, //*(u32*)(r10 - 8 ) = r1
    { 0x18, 1, 0, 0, 0x6c6c6568}, //r1 = b'lleh'
    { 0x00, 0, 0, 0, 0x6f77206f}, //b'ow o'
    //7b 1a e0 ff 00 00 00 00 *(u64 *)(r10 - 16) = r1
    { 0x7b, 0xa, 0x1, 0xfff0, 0}, 
    {0xbf, 0x1, 0xa, 0, 0}, //r1 = r10
    {0x07, 0x1, 0, 0, 0xfffffff0}, //r1 += -16
    //b7 02 00 00 08 00 00 00 r2 = 13
    {0xb7, 2, 0, 0, 0x0d},
    //85 00 00 00 06 00 00 00 call 6
    {0x85, 0, 0, 0, 0x06},
    {0xb7, 0, 0, 0, 0x1},//r0 = 1
    {0x95, 0, 0, 0, 0x0 }, //exit;
};
```

如何运行？
编译 -》 加载到内核
一般需要关联到内核事件，内核整个是由事件驱动的。查看man 发现run test CMD

运行结果查看
