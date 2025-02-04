import matplotlib.pyplot as plt
import numpy as np
from scipy.stats import norm
from functools import reduce
import sys  
import subprocess
import argparse
import pandas as pd
from statistics import mean, variance, stdev, median_grouped

# ref. https://qiita.com/qsnsr123/items/325d21621cfe9e553c17
plt.rcParams['font.family'] ='sans-serif'
plt.rcParams['xtick.direction'] = 'in'
plt.rcParams['ytick.direction'] = 'in'
plt.rcParams['xtick.major.width'] = 1.0
plt.rcParams['ytick.major.width'] = 1.0
plt.rcParams['font.size'] = 9
plt.rcParams['xtick.labelsize'] = 7
plt.rcParams['ytick.labelsize'] = 8
plt.rcParams['axes.linewidth'] = 1.0

# plt.gca().xaxis.get_major_formatter().set_useOffset(True)
# plt.locator_params(axis='x',nbins=3)
# plt.gca().yaxis.set_tick_params(which='both', direction='in',bottom=True, top=True, left=True, right=True)


func_types =[
  "Publication_publish",
  "Publisher_publish",
  "Publication_enqueueMessage",

  "SubscriptionQueue_push",
  "SubscriptionQueue_call_before_callback",
  "SubscriptionQueue_call_after_callback",

  "TransportTCP_write",
  "TransportTCP_read",

  "accept",
  "connect",
  "bind",
  ]

# 2 1613531009784539434 11345 11361 2 2
def collect(name):
    node = dict()
    node["Publication_publish"] = dict()
    node["Publisher_publish"] = dict()
    node["Publication_enqueueMessage"] = dict()
    node["SubscriptionQueue_push"] = dict()
    node["SubscriptionQueue_call_before_callback"] = dict()
    node["SubscriptionQueue_call_after_callback"] = dict()
    node["TransportTCP_write"] = dict()
    node["TransportTCP_read"] = dict()
    node["socket"] = dict()
    node["port"] = list()
    
    with open(name) as f:
        for _l in f.readlines():
            l = _l.split()

            f_type = func_types[int(l[0])]
            seq1 = int(l[4])
            seq2 = int(l[5])
            time = int(l[1])
            pid = int(l[2])
            tid = int(l[3])
            name = l[6]

            if f_type == "Publisher_publish":
                node["Publisher_publish"][seq2] = dict()
                node["Publisher_publish"][seq2]["time"] = time

            elif f_type == "Publication_publish":
                node["Publication_publish"][seq2] = dict()
                node["Publication_publish"][seq2]["time"] = time

            elif f_type == "Publication_enqueueMessage":
                node["Publication_enqueueMessage"][seq1] = dict()
                node["Publication_enqueueMessage"][seq1]["time"] = time 
                node["Publication_enqueueMessage"][seq1]["seq"] = seq2 

            elif f_type == "SubscriptionQueue_push":
                node["SubscriptionQueue_push"][seq1] = dict()
                node["SubscriptionQueue_push"][seq1]["time"] = time 
                node["SubscriptionQueue_push"][seq1]["seq"] = seq2 

            elif f_type == "SubscriptionQueue_call_before_callback":
                node["SubscriptionQueue_call_before_callback"][seq1] = dict()
                node["SubscriptionQueue_call_before_callback"][seq1]["time"] = time 
                node["SubscriptionQueue_call_before_callback"][seq1]["seq"] = seq2 
                node["SubscriptionQueue_call_before_callback"][seq1]["name"] = name 

            elif f_type == "SubscriptionQueue_call_after_callback":
                node["SubscriptionQueue_call_after_callback"][seq1] = dict()
                node["SubscriptionQueue_call_after_callback"][seq1]["time"] = time 

            elif f_type == "TransportTCP_write":
                node["TransportTCP_write"][seq1] = dict()
                node["TransportTCP_write"][seq1]["time"] = time 

            elif f_type == "TransportTCP_read":
                sock = seq1
                seq = seq2
                if not seq in node["TransportTCP_read"].keys():
                    node["TransportTCP_read"][seq] = dict()
                node["TransportTCP_read"][seq][sock] = dict()
                node["TransportTCP_read"][seq][sock]["time"] = time 

            elif f_type == "accept":
                if not seq2 in node["socket"].keys():
                    node["socket"][seq2] = dict()
                node["socket"][seq2]["to"] = seq1

            elif f_type == "connect":
                if not seq2 in node["socket"].keys():
                    node["socket"][seq2] = dict()
                node["socket"][seq2]["to"] = seq1

            elif f_type == "bind":
                if not seq2 in node["socket"].keys():
                    node["socket"][seq2] = dict()
                node["socket"][seq2]["from"] = seq1
                node["port"].append(seq1)

            else:
                print("Error")
    return node

def calc_e2e_intra(node,option):
    lat = dict()

    for seq, t in node["SubscriptionQueue_push"].items():
        time1 = t["time"]
        seq_p = t["seq"]

        try:
            time2 = node["SubscriptionQueue_call_before_callback"][seq_p]["time"]
            name  = node["SubscriptionQueue_call_before_callback"][seq_p]["name"]
        except:
            continue

        if seq in node["TransportTCP_read"].keys():
            continue

        if not name in lat.keys():
            lat[name] = dict()
            lat[name]["sub queue"] = list()

        lat[name]["sub queue"].append(time2 - time1)
    return lat

def calc_e2e_inter(node1,node2, option):
    lat = dict()
    timeline = dict()

    for seq, t in node1["Publisher_publish"].items():
        time0 = t["time"]

        try:
            time1 = node1["Publication_publish"][seq]["time"]
        except:
            continue

        try:
            time2 = node1["Publication_enqueueMessage"][seq]["time"] # time2-1: pub_queue
            seq_e = node1["Publication_enqueueMessage"][seq]["seq"] # time2-1: pub_queue

            time3 = node1["TransportTCP_write"][seq_e]["time"]
        except:
            continue

        try:
            node2["TransportTCP_read"][seq_e]
        except:
            continue

        time4 = 0
        for sock, t in node2["TransportTCP_read"][seq_e].items():
            try:
                if node2["socket"][sock]["to"] in node1["port"]:
                    time4 = t["time"]
                    pass
            except:
                continue
        if time4 - time3 < 0:
            continue
        
        try:
            time5 = node2["SubscriptionQueue_push"][seq_e]["time"]
            seq_p = node2["SubscriptionQueue_push"][seq_e]["seq"]

            time6 = node2["SubscriptionQueue_call_before_callback"][seq_p]["time"]
            name = node2["SubscriptionQueue_call_before_callback"][seq_p]["name"]
        except:
            continue

        if not name in lat.keys():
            lat[name] = dict()
            lat[name]["pub queue"] = list()
            lat[name]["kernel"] = list()
            lat[name]["sub queue"] = list()

            timeline[name] = dict()
            timeline[name]["pub queue"] = list()
            timeline[name]["kernel"] = list()
            timeline[name]["sub queue"] = list()

            if not hasattr(option,"ros_detail") or not option.ros_detail:
                lat[name]["ROS"] = list()
            else:
                lat[name]["ROS: app - pub queue"] = list()
                lat[name]["ROS: pub queue - send"] = list()
                lat[name]["ROS: recv - sub queue"] = list()

                timeline[name]["ROS: app - pub queue"] = list()
                timeline[name]["ROS: pub queue - send"] = list()
                timeline[name]["ROS: recv - sub queue"] = list()

        lat[name]["pub queue"].append(time2 - time1)
        lat[name]["kernel"].append(time4 - time3)
        lat[name]["sub queue"].append(time6 - time5)

        timeline[name]["pub queue"].append(time1)
        timeline[name]["kernel"].append(time3)
        timeline[name]["sub queue"].append(time5)

        if not hasattr(option,"ros_detail") or not option.ros_detail:
            lat[name]["ROS"].append(time6 - time0 - (time6 - time5) - (time4 - time3) - (time2 - time1))
        else:
            lat[name]["ROS: app - pub queue"].append(time1 - time0)
            lat[name]["ROS: pub queue - send"].append(time3 - time2)
            lat[name]["ROS: recv - sub queue"].append(time5 - time4)

            timeline[name]["ROS: app - pub queue"].append(time0)
            timeline[name]["ROS: pub queue - send"].append(time2)
            timeline[name]["ROS: recv - sub queue"].append(time4)

    return lat,timeline

def show_inter(args):
    n1 = collect(args.src_node)
    n2 = collect(args.dst_node)

    lat,timeline = calc_e2e_inter(n1,n2, args)

    node_num = len(lat)
    if node_num == 0:
        print("None")
        sys.exit(0)

    time_num = len(list(lat.values())[0])

    if args.concat:
        fig, ax = plt.subplots(node_num, time_num, sharex="row", sharey="row",squeeze=False)
    else:
        fig, ax = plt.subplots(node_num, 1, sharex="row", sharey="row",squeeze=False)

    for i,(node_name, node_time) in enumerate(lat.items(), start=0):
        assert reduce(lambda x,y: x if len(x) == len(y) else False, node_time.values())
        try:
            demangler_out = subprocess.run(["./demangler", node_name], capture_output=True)
            node_name = demangler_out.stdout.decode('utf-8')
        except:
            print("error: demangler")

        print()
        print(node_name)
        print("num: ", len(list(node_time.values())[0]))

        for j,(lat_name, time)  in enumerate(node_time.items()):
            if args.concat:
                ax[i,j].hist(time, bins =50, histtype = 'bar')
                ax[i,j].set_title(lat_name)
            else:
                ax[i,0].hist(time, bins =50, histtype = 'bar', label = lat_name)
                ax[i,0].legend(loc="best")

    plt.show()

def show_intra(args):
    n = collect(args.node)

    lat = calc_e2e_intra(n,args)

    fig = plt.figure()

    node_num = len(lat)

    if node_num == 0:
        print("None")
        sys.exit(0)

    for i,(node_name, node_time) in enumerate(lat.items(), start=0):
        assert reduce(lambda x,y: x if len(x) == len(y) else False, node_time.values())
        try:
            demangler_out = subprocess.run(["./demangler", node_name], capture_output=True)
            node_name = demangler_out.stdout.decode('utf-8')
        except:
            print("error: demangler")

        print()
        print(node_name)
        print("num: ", len(list(node_time.values())[0]))

        time_num = len(node_time)
        for j,(lat_name, time)  in enumerate(node_time.items(), start=1):
            if args.concat:
                ax = fig.add_subplot(node_num, time_num, time_num*i+j)
            else:
                ax = fig.add_subplot(node_num,1,1)
            ax.hist(time, bins =50, histtype = 'bar', label = lat_name)
            ax.legend(loc="best")

    plt.show()

def show_stack(args):
    latencies = list()
    lat_source = list()
    index = list()

    for nodes in args.nodes:
        n1 = collect(nodes[0])
        n2 = collect(nodes[1])

        lat,timeline = calc_e2e_inter(n1,n2,args)

        index.append(nodes[2])

        node_num = len(lat)
        if node_num != 1:
            print("Error: the number of topic must be 1. ",nodes[2])
            sys.exit(0)

        for i,(node_name, node_time) in enumerate(lat.items(), start=0):
            assert reduce(lambda x,y: x if len(x) == len(y) else False, node_time.values())
            try:
                demangler_out = subprocess.run(["./demangler", node_name], capture_output=True)
                node_name = demangler_out.stdout.decode('utf-8')
            except:
                print("error: demangler")

            print()
            print(node_name)
            print("num: ", len(list(node_time.values())[0]))

            lat = list()
            lat_source = list()
            for j,(lat_name, time)  in enumerate(node_time.items(), start=1):
                if args.kind == "mean":
                    lat.append(mean(time))
                elif args.kind == "median":
                    lat.append(median_grouped(time))
                elif args.kind == "tail":
                    lat.append(max(time))
                elif args.kind == "stdev":
                    lat.append(stdev(time))
                elif args.kind == "99percentile":
                    lat.append(np.percentile(time,99))
                lat_source.append(lat_name)

            latencies.append(lat)

    fig, ax = plt.subplots()
    data = pd.DataFrame(latencies, index=index, columns=lat_source)
    data.plot(kind='bar', stacked=True, ax=ax)
    plt.show()

def show_timeline(args):
    n1 = collect(args.src_node)
    n2 = collect(args.dst_node)

    if args.pickup == None:
        print("Please select: kernel, pubq, subq, ros")
        sys.exit(0)
    pickup_num = len(args.pickup)

    lat, timeline = calc_e2e_inter(n1,n2, args)

    node_num = len(lat)
    if node_num == 0:
        print("None")
        sys.exit(0)

    time_num = len(list(lat.values())[0])

    fig, ax = plt.subplots(node_num, pickup_num, sharex="row", sharey="row",squeeze=False)

    for i,(node_name, node_time) in enumerate(lat.items(), start=0):
        assert reduce(lambda x,y: x if len(x) == len(y) else False, node_time.values())
        try:
            demangler_out = subprocess.run(["./demangler", node_name], capture_output=True)
            node_name_demangle = demangler_out.stdout.decode('utf-8')
        except:
            print("error: demangler")
            node_name_demangle = ""

        print()
        print(node_name, " (", node_name_demangle, ")")
        print("num: ", len(list(node_time.values())[0]))

        for j,key in enumerate(args.pickup):
            ax[i,j].plot(timeline[node_name][key], lat[node_name][key])
            ax[i,j].set_title(key)

    plt.show()


parser = argparse.ArgumentParser()
subparsers = parser.add_subparsers()

parser_inter = subparsers.add_parser('inter')
parser_inter.add_argument("src_node", metavar="<src node>")
parser_inter.add_argument("dst_node", metavar="<dst node>")
parser_inter.add_argument("--ros-detail", dest="ros_detail", action='store_true')
parser_inter.add_argument("-c", dest="concat", action='store_true')
parser_inter.set_defaults(handler=show_inter)

parser_intra = subparsers.add_parser('intra')
parser_intra.add_argument("node", metavar="<node>")
parser_intra.add_argument("--ros-detail", dest="ros_detail", action='store_true')
parser_intra.add_argument("-c", dest="concat", action='store_true')
parser_intra.set_defaults(handler=show_intra)

parser_stack = subparsers.add_parser('stack')
parser_stack.add_argument("--nodes", "-n", metavar=("src","dst","name"), nargs=3, action="append", required=True)
parser_stack.add_argument("--ros-detail", dest="ros_detail", action='store_true')
parser_stack.add_argument("--kind","-k", choices=["median","mean","tail","stdev","99percentile"], default = "mean")
parser_stack.set_defaults(handler=show_stack)

parser_timeline = subparsers.add_parser('timeline')
parser_timeline.add_argument("src_node", metavar="<src node>")
parser_timeline.add_argument("dst_node", metavar="<dst node>")
parser_timeline.add_argument('--kernel', dest='pickup', action='append_const', const="kernel")
parser_timeline.add_argument('--pubq', dest='pickup', action='append_const', const="pub queue")
parser_timeline.add_argument('--subq', dest='pickup', action='append_const', const="sub queue")
parser_timeline.set_defaults(handler=show_timeline)

args = parser.parse_args()
if hasattr(args, 'handler'):
    args.handler(args)
else:
    print("Command line parse error")
