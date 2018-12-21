#!/usr/bin/env python3

# Copyright (c) 2018 Kevin Weiss, for HAW Hamburg  <kevin.weiss@haw-hamburg.de>
# Copyright (C) 2018 Peter Kietzmann, for HAW Hamburg <peter.kietzmann@haw-hamburg.de>
#
# This file is subject to the terms and conditions of the GNU Lesser
# General Public License v2.1. See the file LICENSE in the top level
# directory for more details.

import argparse
import numpy
import os
import sys

DEFAULT_POWER_CYCLES = 500
DEFAULT_OFF_TIME = 1
DEFAULT_BAUDRATE = 115200
DEFAULT_PORT = '/dev/ttyUSB0'
DEFAULT_INFO = True

def bit_probability(all_meas):
    p1 = numpy.zeros(len(all_meas[0]))
    # number of ones for each bit
    for i in range(0, len(all_meas[0])):
        tmp = list(map(lambda x: int(x[i]), all_meas))
        p1[i] = numpy.count_nonzero(tmp)

    # probability of ones
    p1 = numpy.divide(p1, float(len(all_meas)))
    # probability of zeros
    p0 = 1 - p1
    return [p0, p1]

def min_erntropy(all_meas):
    [p0, p1] = bit_probability(all_meas)
    p0_1_max = numpy.maximum(p1, p0)
    log2_p0_1_max = numpy.log2(p0_1_max)
    H_min = numpy.sum(-log2_p0_1_max)
    H_min_rel = 100 * H_min/len(p0_1_max)

    return [H_min, H_min_rel]

def biterrs(all_meas):
    [p0, p1] = bit_probability(all_meas)

    biterr = 0
    for p1_bit in p1:
        # bitflip when occured when prob. is not 100% or 0%
        if (p1_bit != 1.0) and (p1_bit != 0.0):
            if (p1_bit < .5):
                biterr += p1_bit
            else:
                biterr += (1-p1_bit)
    # normalize to word length and percent
    id_err = 100 * biterr / len(all_meas[0])
    return id_err


def main_func():
    p = argparse.ArgumentParser()
    p.add_argument("-n", "--number", type=int, default=DEFAULT_POWER_CYCLES,
                   help="Number of iterations, default: %s" % DEFAULT_POWER_CYCLES)
    p.add_argument("-t", "--off_time", type=int, default=DEFAULT_OFF_TIME,
                   help="Off time, default: %s [s]" % DEFAULT_OFF_TIME)
    p.add_argument("-p", "--port", type=str, default=DEFAULT_PORT,
                   help="Serial port, default: %s" % DEFAULT_PORT)
    p.add_argument("-b", "--baudrate", type=int, default=DEFAULT_BAUDRATE,
                   help="Baudrate of the serial port, default: %d" % DEFAULT_BAUDRATE)
    p.add_argument("-d", "--disable_output", default=DEFAULT_INFO, action='store_false',
                   help="Disable verbose output")
    args = p.parse_args()

    puf_sram = puf_sram_if.PufSram(port=args.port, baud=args.baudrate)
    data = puf_sram.get_data_list(n=args.number, off_time=args.off_time,
                                    allow_print=args.disable_output)


    print("Number of measurements: %i       " % len(data[0]))

    # evaluate random seeds
    if len(data) >= 1:
        seeds = [format(x, '0>32b') for x in data[0]]
        H_min_seeds, H_min_rel_seeds = min_erntropy(seeds)
        print("Seed length           : %i Bit   " % len(seeds[0]))
        print("Seed abs. Entropy     : %02.02f Bit   " % H_min_seeds)
        print("Seed rel. Entropy     : %02.02f perc. " % H_min_rel_seeds)

    # evaluate IDs
    if len(data) >= 2:
        ids = [format(x, '0>160b') for x in data[1]]
        error_prob = biterrs(ids)
        print("ID length             : %i Bit   " % len(ids[0]))
        print("ID error probability  : %02.02f perc. " % error_prob)

if __name__ == "__main__":
    sys.path.append(os.path.join('../../dist/tools/puf-sram'))
    import puf_sram_if
    main_func()
