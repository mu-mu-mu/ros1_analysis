from __future__ import print_function
from bcc import BPF
from time import strftime

libroscpp_path = "/opt/ros/noetic/lib/libroscpp.so"

# load BPF program
bpf_text = """
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>

int new_roscpp_program(struct pt_regs *ctx) {
    char comm[TASK_COMM_LEN]; //16文字までしか取れないっぽい
    bpf_get_current_comm(&comm, sizeof(comm));  //この関数でcommandを取得

    uint64_t pid = bpf_get_current_pid_tgid();

    bpf_trace_printk("new node started: %s pid/tgid: %d", comm, pid);
    return 0;
}

"""
b = BPF(text=bpf_text)

# nm -D /opt/ros/noetic/lib/roscpp.so | c++filt | grep ros::init
b.attach_uprobe(name=libroscpp_path,
        sym="_ZN3ros4initERiPPcRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEj", fn_name="new_roscpp_program")


# format output
while 1:
    try:
        (task, pid, cpu, flags, ts, msg) = b.trace_fields()
    except ValueError:
        continue
    print("%s" %  msg)
