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
    median = []
    std = [] # standard deviation
    identical = [] # whenever identical keys where found

    for i, x in enumerate(codes_str):
        print("\n\n- - - - - - - - - - - - - - - - - - - -\n  key[" + str(i) + "]:\n")
        h_list = []

        for j, y in enumerate(codes_str):
            if j != i:
                h = hamming(codes_str[i], codes_str[j])
                h_list.append(h)
                if (h == 0):
                    identical.append("["+str(i)+"]&["+str(j)+"]")

        h_mean = np.mean(h_list)
        h_median = np.median(h_list)
        h_min = np.min(h_list)
        h_max = np.max(h_list)
        h_std = np.std(h_list)

        mean.append(h_mean)
        median.append(h_median)
        std.append(h_std)

        print("  h_min    = ", h_min)
        print("  h_max    = ", h_max)
        print("  h_mean   = ", h_mean)
        print("  h_median = ", h_median)
        print("  h_std    = ", h_std)
    # for-end

    print("\n\n  Calculated over all *mean* data:")
    print("  min    = ", np.min(mean))
    print("  max    = ", np.max(mean))
    print("  mean   = ", np.mean(mean))
    print("  median = ", np.median(mean))
    print("  std    = ", np.std(mean))

    print("\n\n  Calculated over all *median* data:")
    print("  min    = ", np.min(median))
    print("  max    = ", np.max(median))
    print("  mean   = ", np.mean(median))
    print("  median = ", np.median(median))
    print("  std    = ", np.std(median))

    print("\n\n  Calculated over all *standard deviation* data:")
    print("  min    = ", np.min(std))
    print("  max    = ", np.max(std))
    print("  mean   = ", np.mean(std))
    print("  median = ", np.median(std))
    print("  std    = ", np.std(std))

    if identical:
        print("  identical keys : ", identical)


if __name__ == "__main__":
    main_func()
