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
    for i, x in enumerate(conc):
        print("\n\n Index\t: Key\t: hamming-distance[h]\n-----------------\n  ["+str(i)+"]\t: "+str(x)+"\n-----------------")
        for j, y in enumerate(conc):
            if j != i:
                h = hamming(conc[i], conc[j])
                print("  [" + str(j) + "]\t: " + str(y) + " : h = ", h)
                if (h == 0):
                    print("\t-> Identical key found!")


if __name__ == "__main__":
    main_func()
