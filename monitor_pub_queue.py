from __future__ import print_function
from bcc import BPF
from time import strftime

libroscpp_path = "/opt/ros/noetic/lib/libroscpp.so"

# load BPF program
bpf_text = """
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>

int enqueueMessage(struct pt_regs *ctx) {
    uint64_t pid = bpf_get_current_pid_tgid();
    bpf_trace_printk("enqueueMessage() called: msg addr - %p :pid/tid %d", (void *)ctx->di, pid);
    return 0;
}

int publish(struct pt_regs *ctx) {
    uint64_t pid = bpf_get_current_pid_tgid();
    bpf_trace_printk("publish() called: msg addr - %p :pid/tid %d", (void *)ctx->di, pid);
    return 0;
}

"""
b = BPF(text=bpf_text)

# Publication::enqueueMessage(ros::SerializedMessage const&)
b.attach_uprobe(name=libroscpp_path,
        sym="_ZN3ros11Publication14enqueueMessageERKNS_17SerializedMessageE", fn_name="enqueueMessage")

# publication::publish()
b.attach_uprobe(name=libroscpp_path,
        sym="_ZN3ros11Publication7publishERNS_17SerializedMessageE", fn_name="publish")

# format output
while 1:
    try:
        (task, pid, cpu, flags, ts, msg) = b.trace_fields()
    except ValueError:
        continue
    print("%s" % msg)
