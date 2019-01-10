#!/usr/bin/env python3

# Copyright (C) 2018 HAW Hamburg
#
# This file is subject to the terms and conditions of the GNU Lesser
# General Public License v2.1. See the file LICENSE in the top level
# directory for more details.
#
# Author:   Peter Kietzmann <peter.kietzmann@haw-hamburg.de>

import os
import sys
import numpy
import random
import puf_sram_if
from ctypes import *

def coder1(data):
    # include coder from pre-compiled c-code TODO re-compile this w/o debug msg
    golay_coder = CDLL(os.path.join(os.environ['RIOTTOOLS'],'puf-sram/fec_golay2412.so'))
    # allocate uint8_t array
    data = (c_uint8 * len(data))(*data)
    p_data = pointer(data)
    # allocate buffer for golay encoded symbol
    enc_buf  = (c_uint8 * 2*len(data))()
    p_enc_buf  = pointer(enc_buf)
    # encode input data
    golay_coder.fec_golay2412_encode(len(data), p_data, p_enc_buf)
    # reshape array
    enc_buf = numpy.asarray(enc_buf).reshape(-1)
    return enc_buf

def coder2(data):
    # include coder from pre-compiled c-code
    repetition_coder = CDLL(os.path.join(os.environ['RIOTTOOLS'],'puf-sram/repetition.so')) #11
    # allocate uint8_t array
    data = (c_uint8 * len(data))(*data)
    p_data = pointer(data)
    # allocate buffer for repetition encoded symbol
    enc2_buf  = (c_uint8 * 11*len(data))()
    p_enc2_buf  = pointer(enc2_buf)
    # encode repetition
    repetition_coder.repetition_encode(len(data), p_data, p_enc2_buf)
    # reshape array
    enc2_buf = numpy.asarray(enc2_buf).reshape(-1)
    return enc2_buf

def xor_arrays(a, b):
    assert len(a) == len(b)
    c = numpy.zeros(len(a))
    for i in range(len(c)):
        c[i] = (a[i] ^ b[i])
    return c

def calc_mle_puf(data):
    sum_puf_res = numpy.zeros(len(data[0]) * 8)
    ref_puf_hex=[]
    # for each measurement
    for puf in data:
        # convert each puf byte to binay representation
        puf_bin = [format(x, '0>8b') for x in puf]
        puf_bin = list("".join(puf_bin))
        # sum measurements
        sum_puf_res = numpy.add(sum_puf_res, numpy.asarray(puf_bin, dtype=float))
    # binarize maximum to likelihood
    for i, p_bit in enumerate(sum_puf_res):
        if (float(p_bit)/len(data)) > .5:
            sum_puf_res[i] = 1
        else:
            sum_puf_res[i] = 0
    # build bytes
    sum_puf_res = numpy.packbits(sum_puf_res.astype(numpy.uint8))

    return sum_puf_res


if __name__ == "__main__":

    # default values for external FTDI can be overwritten
    helper_port = os.getenv('PORT2', '/dev/ttyUSB0')
    helper_baud = os.getenv('BAUD2', 115200)
    helper_off_reps = os.getenv('REPS', 3)
    helper_offtime = os.getenv('OFFTIME', 1)
    helper_print = os.getenv('PRINT', 1)

    # default value for code offset
    code_offset = os.getenv('CODEOFFSET', 6)

    # calc max. likely PUF response based on multiple mesurements
    puf_sram = puf_sram_if.PufSram(port=helper_port, baud=helper_baud)
    puf_responses = puf_sram.get_puf_responses(n=helper_off_reps, off_time=helper_offtime,
                                               allow_print=helper_print)
    puf_reference = calc_mle_puf(puf_responses)

    # generate random code offset
    bytes = random.sample(range(0, 255), code_offset)

    # golay
    enc_buf = coder1(bytes)

    # repetion
    enc2_buf = coder2(enc_buf)

    # xor
    helper = xor_arrays(enc2_buf, puf_reference)

    # save helper on eeprom via shell command
    puf_sram.write_helper((helper.astype(int)));
