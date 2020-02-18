#!/usr/bin/env python3


# Copyright (C) 2020 René Herthel, for HAW Hamburg <rene.herthel@haw-hamburg.de>
#
# This file is subject to the terms and conditions of the GNU Lesser
# General Public License v2.1. See the file LICENSE in the top level
# directory for more details.

import numpy as np

def hamming(str_x, str_y):
    # make sure its the same length
    assert len(str_x) == len(str_y)
    # count (sum) every different bit
    return sum(bit_x != bit_y for bit_x, bit_y in zip(str_x, str_y))

def main_func():
    codes_int = []
    with open('log.txt') as f:
        # Every single line in log.txt
        for line in f:
            # Check if the line contains start- and end-marker
            if (("idstart{" in line) and ("}idend" in line)):
                    # Extract string between braces, only containing the values
                    id = line[line.find("{")+1:line.find("}")].split()
                    if id:
                        # Convert each entry into an integer
                        id = [int(i) for i in id]
                        print("Key found : ", id)
                        codes_int.append(id)

    print("Key's total : ", str(len(codes_int)))

    # convert into binary-string
    codes_bin = [[format(byte, '0>8b') for byte in code] for code in codes_int]

    # concatenate the id's single values into one large 'bit'-string
    codes_str = list()
    for i in range(len(codes_bin)):
        codes_str.append(''.join(codes_bin[int(i)]))

    mean = []
    identical = []
    for i, x in enumerate(codes_str):
        #print("\n\n Index\t: Key\t: hamming-distance[h]\n-----------------\n  ["+str(i)+"]\t: "+str(x)+"\n-----------------")
        print("\n\n Index\t: Key\t: hamming-distance[h]\n-----------------\n  ["+str(i)+"]\t: "+str(x))
        h_list = []
        for j, y in enumerate(codes_str):
            if j != i:
                h = hamming(codes_str[i], codes_str[j])
                h_list.append(h)
                #print("  [" + str(j) + "]\t: " + str(y) + " : h = ", h)
                if (h == 0):
                    #print("\t-> Identical key found! ["+str(i)+"] & ["+str(j)+"]\n")
                    identical.append("["+str(i)+"]&["+str(j)+"]")

        h_mean = sum(h_list) / len(h_list)
        print("\t-> h_mean = ", h_mean)
        mean.append(h_mean)
    # for-end

    print("\n\nEndresult:")
    print(" - mean overall : ", sum(mean) / len(mean))
    print(" - identical keys : ", identical)

if __name__ == "__main__":
    main_func()
