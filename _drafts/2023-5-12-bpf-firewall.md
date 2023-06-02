build:
clang -g -O2 -target bpf -c bpf.c -o bpf.o
 bpftool prog load ./bpf.o /sys/fs/bpf/cgroup_firewall type cgroup/skb
bpftool cgroup attach /sys/fs/cgroup/user.slice/user-1000.slice/session-3023.scope/ egress pinned /sys/fs/bpf/cgroup_firewall multi
bpftool cgroup detach /sys/fs/cgroup/user.slice/ ingress id 933
