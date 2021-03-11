import matplotlib.pyplot as plt
import numpy as np
from scipy.stats import norm
#func_types = ["publish", "enqueueMessage", "push", "call"]

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

            elif f_type == "SubscriptionQueue_call_before_callback":
                node["SubscriptionQueue_call_before_callback"][seq1] = dict()
                node["SubscriptionQueue_call_before_callback"][seq1]["time"] = time 

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

# n1 = collect('./log_node1_624693')
# n2 = collect('./log_node2_624694')
# n3 = collect('./log_node3_624700')
# n4 = collect('./log_node4_624703')

n1 = collect('./log_node1_623154')
n2 = collect('./log_node2_623155')
n3 = collect('./log_node3_623160')
n4 = collect('./log_node4_623164')

def calc_e2e(node1,node2):
    t1 = []
    t2 = []
    t3 = []

    for seq, t in node1["Publication_publish"].items():
        #try:
            time1 = t["time"]

            time2 = node1["Publication_enqueueMessage"][seq]["time"] # time2-1: pub_queue
            seq_k = node1["Publication_enqueueMessage"][seq]["seq"] # time2-1: pub_queue

            time3 = node1["TransportTCP_write"][seq_k]["time"]

            time4 = 0
            for sock, t in node2["TransportTCP_read"][seq_k].items():
                try:
                    if node2["socket"][sock]["to"] in node1["port"]:
                        time4 = t["time"]
                        pass
                except:
                    continue
            if time4 - time3 < 0:
                continue
                
            
            time5 = node2["SubscriptionQueue_push"][seq_k]["time"]
            time6 = node2["SubscriptionQueue_call_before_callback"][seq_k]["time"]

            if time2 - time1 > 400000: # Tail Latency?
                continue
            t1.append(time2 - time1)
            t2.append(time4 - time3)
            t3.append(time6 - time5)
    return t1,t2,t3

t1,t2,t3 = calc_e2e(n3,n4)

print(len(t1))
print(len(t2))
print(len(t3))

plt.hist(t3, bins =50, histtype = 'bar', label = "sub queue")
plt.hist(t2, bins =50, histtype = 'bar', label = "kernel")
plt.hist(t1, bins =50, histtype = 'bar', label = "pub queue")
plt.legend(loc="best")
plt.show()
