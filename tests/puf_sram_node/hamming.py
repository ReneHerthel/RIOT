#!/usr/bin/env python3


# Copyright (C) 2020 René Herthel, for HAW Hamburg <rene.herthel@haw-hamburg.de>
#
# This file is subject to the terms and conditions of the GNU Lesser
# General Public License v2.1. See the file LICENSE in the top level
# directory for more details.

import numpy as np

def hamming(s1, s2):
    assert len(s1) == len(s2)
    return sum(c1 != c2 for c1, c2 in zip(s1, s2))

def main_func():
    id_counter = 0

    # extract the ids/keys from the logfile
    ids_int = []
    with open('log.txt') as f:
        for line in f:
            if (("idstart{" in line) and ("}idend" in line)):
                    id = line[line.find("{")+1:line.find("}")].split()
                    if id:
                        id_counter += 1
                        id = [int(i) for i in id]
                        print("Found Key : ", id)
                        ids_int.append(id)

    print("Key's total == ", str(id_counter))

    # convert into binary-string
    bin_str = [[format(x, '0>8b') for x in id] for id in ids_int]

    # concatenate the id's single values into one large 'bit'-string
    conc = list()
    for i, x in enumerate(bin_str):
        conc.append(''.join(bin_str[i]))

    # take one id. compare to all other ids. print results of hamming distance in comparison with other keys
    mean = []
    median = []
    identical = []
    for i, x in enumerate(conc):
        #print("\n\n Index\t: Key\t: hamming-distance[h]\n-----------------\n  ["+str(i)+"]\t: "+str(x)+"\n-----------------")
        print("\n\n Index\t: Key\t: hamming-distance[h]\n-----------------\n  ["+str(i)+"]\t: "+str(x))
        h_sum = 0
        h_num = 0
        h_list = []
        for j, y in enumerate(conc):
            if j != i:
                h = hamming(conc[i], conc[j])
                h_list.append(h)
                h_sum += h
                h_num += 1
                #print("  [" + str(j) + "]\t: " + str(y) + " : h = ", h)
                if (h == 0):
                    print("\t-> Identical key found! ["+str(i)+"] & ["+str(j)+"]\n")
                    identical.append(tuple((i, j)))
        #print("\t-> h_sum = ", h_sum)
        h_mean = h_sum/h_num
        print("\t-> h_mean = ", h_mean)
        h_len = len(h_list)
        #print("len : ",h_len)
        h_median = 0
        if (((h_len / 2) % 2) == 0 ): # 97 / 2 = 48.5 % 2 != 0
            h_median = h_list[int(h_len/2)]
        else:
            #print("h_list: ",h_list[int(h_len/2)])
            #print("h_list2: ",h_list[int((h_len/2))+1])
            h_median = (h_list[int(h_len/2)]+h_list[int((h_len/2))+1])/2
        print("\t-> h_median = ", h_median)
        mean.append(h_mean)
        median.append(h_median)
        # for-end

    mean_sum = 0
    mean_count = 0
    for i, x in enumerate(mean):
        mean_count += 1
        mean_sum += x

    print("\n\nEndresult:")
    print(" - mean total : ", mean_sum/mean_count)
    print(" - identical keys : ", identical)
    """
    k = 0
    if (((len(median) / 2) % 2) == 0):
        k = median[int(len(median)/2)]
    else:
        k = (median[int(len(median)/2)]+median[int((len(median)/2))+1])/2
    print("median of all : ", k)
    """


if __name__ == "__main__":
    main_func()
