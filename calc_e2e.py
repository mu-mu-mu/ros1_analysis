import matplotlib.pyplot as plt
import numpy as np
from scipy.stats import norm
from functools import reduce
import sys  
import subprocess


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

def calc_e2e_intra(node):
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

def calc_e2e_inter(node1,node2):
    lat = dict()

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
            lat[name]["ROS"] = list()

            #lat[name]["ROS1"] = list()
            #lat[name]["ROS2"] = list()
            #lat[name]["ROS3"] = list()

        lat[name]["pub queue"].append(time2 - time1)
        lat[name]["kernel"].append(time4 - time3)
        lat[name]["sub queue"].append(time6 - time5)
        lat[name]["ROS"].append(time6 - time0 - (time6 - time5) - (time4 - time3) - (time2 - time1))

        #lat[name]["ROS1"].append(time1 - time0)
        #lat[name]["ROS2"].append(time3 - time2)
        #lat[name]["ROS3"].append(time5 - time4)

    return lat

args = sys.argv

if args[1] == "inter":
    if len(args) == 4:
        n1_name = args[2]
        n2_name = args[3]

    elif len(args) == 5:
        n1_name = args[2] + "/" + args[3]
        n2_name = args[2] + "/" + args[4]
    else:
        print("calc_e2e.py inter /path/to/src_node /path/to/dst_node")
        print("calc_e2e.py inter /path/to  src_node dst_node")
        sys.exit(1)

    n1 = collect(n1_name)
    n2 = collect(n2_name)

    lat = calc_e2e_inter(n1,n2)

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

        for j,(lat_name, time)  in enumerate(node_time.items(), start=1):
            ax = fig.add_subplot(node_num, 4, 4*i+j )
            ax.hist(time, bins =50, histtype = 'bar', label = lat_name)
            ax.legend(loc="best")

    plt.show()

elif args[1] == "intra":
    if len(args) == 3:
        n_name = args[2]
    else:
        print("calc_e2e.py intra /path/to/src_node")
        sys.exit(1)

    n = collect(n_name)

    lat = calc_e2e_intra(n)

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

        for j,(lat_name, time)  in enumerate(node_time.items(), start=1):
            ax = fig.add_subplot(node_num, 1, i+j )
            ax.hist(time, bins =50, histtype = 'bar', label = lat_name)
            ax.legend(loc="best")

    plt.show()

else:
    print("inter or intra")
