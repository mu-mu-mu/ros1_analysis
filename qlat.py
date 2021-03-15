from __future__ import print_function
from bcc import BPF
from time import strftime

from qlat_text import *
libroscpp_path = "/opt/ros/noetic/lib/libroscpp.so"

ros_app_pids = dict()
ros_app_pids_delta = dict()

bpf_get_tid = BPF(text=bpf_text_get_tid)

def get_tid_attach(tgid):
    # Publication::enqueueMessage(ros::SerializedMessage const&)
    bpf_get_tid.attach_uprobe(name=libroscpp_path, pid = tgid,
            sym="_ZN3ros11Publication14enqueueMessageERKNS_17SerializedMessageE", fn_name="enqueueMessage")

    # ros::SubscriptionQueue::call()
    bpf_get_tid.attach_uprobe(name=libroscpp_path, pid = tgid,
            sym="_ZN3ros17SubscriptionQueue4callEv", fn_name="publish")

    # ros::SubscriptionQueue::call()
    bpf_get_tid.attach_uprobe(name=libroscpp_path, pid = tgid,
            sym="_ZN3ros11Publication7publishERNS_17SerializedMessageE", fn_name="publish")


def get_tid_detach(tgid, func):
    if (func == "enqueueMessage"):
        try:
            # Publication::enqueueMessage(ros::SerializedMessage const&)
            bpf_get_tid.detach_uprobe(name=libroscpp_path, pid = tgid,
                    sym="_ZN3ros11Publication14enqueueMessageERKNS_17SerializedMessageE")
        except:
            print("detach IO error")

    if (func == "pubsub"):
        try:
            # ros::SubscriptionQueue::call()
            bpf_get_tid.detach_uprobe(name=libroscpp_path, pid = tgid,
                    sym="_ZN3ros17SubscriptionQueue4callEv")

            bpf_get_tid.detach_uprobe(name=libroscpp_path, pid = tgid,
                    sym="_ZN3ros11Publication7publishERNS_17SerializedMessageE")
        except:
            print("detach pubsub error")




def ros_init():
    bpf_get_tid.attach_uprobe(name=libroscpp_path,
            sym="_ZN3ros4initERiPPcRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEj", fn_name="init")

def sched_check():
    bpf_get_tid.attach_kprobe(event="ttwu_do_wakeup", fn_name="trace_ttwu_do_wakeup")
    bpf_get_tid.attach_kprobe(event="wake_up_new_task", fn_name="trace_wake_up_new_task")
    bpf_get_tid.attach_kprobe(event="finish_task_switch", fn_name="trace_run")

def print_get_pid(cpu, data, size):
    event = bpf_get_tid["data_pid"].event(data)

    if event.type == 0:
        print('IOTHREAD detected:  PID- %d, TID- %d '%(event.tgid, event.pid))
        get_tid_detach(event.tgid, "enqueueMessage")
        ros_app_pids[event.pid] = "IOTHREAD" 
        ros_app_pids_delta[event.pid] = [] 

    elif event.type == 1:
        print('CALLBACKTHREAD detected:  PID- %d, TID- %d '%(event.tgid, event.pid))
        get_tid_detach(event.tgid, "pubsub")
        ros_app_pids[event.pid] = "CALLBACKTHREAD" 
        ros_app_pids_delta[event.pid] = []


def print_init(cpu, data, size):
    event = bpf_get_tid["data_init"].event(data)
    tgid = event.tgid
    get_tid_attach(tgid)

def print_delta(cpu, data, size):
    event = bpf_get_tid["data_delta"].event(data)
    ros_app_pids_delta[event.pid].append(event.ns)

bpf_get_tid["data_pid"].open_perf_buffer(print_get_pid)
bpf_get_tid["data_init"].open_perf_buffer(print_init)
bpf_get_tid["data_delta"].open_perf_buffer(print_delta)

ros_init()
sched_check()

while 1:
    try:
        bpf_get_tid.perf_buffer_poll()
    except KeyboardInterrupt:
        for pid, vals in ros_app_pids_delta.items():
            with open("data/" + ros_app_pids[pid] + str(pid) + ".txt", "w") as f:
                f.write("\n".join(map(lambda x: "Q " + str(x), vals)))
        exit()
