#!/usr/bin/env python3

# Copyright (C) 2018 Kevin Weiss <kevin.weiss@haw-hamburg.de>
#
# This file is subject to the terms and conditions of the GNU Lesser
# General Public License v2.1. See the file LICENSE in the top level
# directory for more details.

import serial
import time

import numpy
class PufSram:

    def __init__(self, port, baud):
        self.__dev = serial.Serial(port, baud, timeout=10)
        if(self.__dev.isOpen() is False):
            self.__dev.open()

    def repower(self, shutdown_time):
        self.__dev.setRTS(True)
        time.sleep(shutdown_time)
        self.__dev.setRTS(False)

    def write_helper(self, helper):
        # split helper array in chunks because of reduced RIOT shell buffer
        helper = numpy.array_split(helper, round(len(helper)/6))
        memory_position = 0
        for chunk in helper:
            chunk_str=' '.join(map(str, chunk))
            cmd = ('write '+str(memory_position)+' '+chunk_str+'\n').encode()
            self.__dev.write(cmd)
            memory_position += len(chunk)
            # give RIOT shell some time
            time.sleep(.05)

    def read_data(self):
        data = None
        start = False
        str = 'no_exit'
        while (str != ''):
            str = self.__dev.readline()
            if (b'Start: ' in str):
                start = True
            if ((b'Success: ' in str) and (start is True)):
                if (b'[' in str) and (b']' in str):
                    data_str = str[str.find(b"[")+1:str.find(b"]")]
                    data = int(data_str, 0)
            if ((b'End: ' in str) and (data is not None)):
                return data
        return None

    def read_data2(self):
        id_val = None
        out= list()
        start = False
        str = 'no_exit'
        while (str != ''):
            str = self.__dev.readline()
            if (b'Start: ' in str):
                start = True
            if ((b'Success: ' in str) and (start is True)):
                if (b'[' in str) and (b']' in str):
                    data_str = str[str.find(b"[")+1:str.find(b"]")]
                    data = data_str.split()
                    for i, val in enumerate(data):
                        out.append(int(data_str.split()[i], 0))
            if ((b'End: ' in str) and (data is not None)):
                return out
        return None

    def read_memory(self):
        data = None
        start = False
        str = 'no_exit'
        while (str != ''):
            str = self.__dev.readline()
            if (b'Start: ' in str):
                start = True
            if ((b'Success: ' in str) and (start is True)):
                if (b'[' in str) and (b']' in str):
                    data_str = str[str.find(b'[')+1:str.find(b']')]
                    data = (data_str.split(b' '))
                    # get rid of last finding due to final whitespace
                    data = data[:-1]
                    data = [int(val, 0) for val in data]
            if ((b'End: ' in str) and (data is not None)):
                return data
        return None

    def get_seed_list(self, n, off_time, allow_print):
        data = list()
        for i in range(0, n):
            self.repower(off_time)
            data.append(self.read_data())
            if (allow_print):
                print('Iteration %i/%i' % (i+1, n))
                print(data[-1])
        return data

    def get_data_list(self, n, off_time, allow_print):
        seeds = list()
        ids = list()
        for i in range(0, n):
            self.repower(off_time)
            data = self.read_data2()
            seeds.append(data[0])
            ids.append(data[1])
            if (allow_print):
                print('Iteration %i/%i' % (i+1, n))
                print('seed %i' % seeds[-1])
                print('id   %i' % ids[-1])
        return [seeds, ids]

    def get_puf_responses(self, n, off_time, allow_print):
        data = list()
        for i in range(0, n):
            self.repower(off_time)
            data.append(self.read_memory())
            if (allow_print):
                print('Iteration %i/%i' % (i+1, n))
                print(data[-1])
        return data

    def connect(self, dev):
        if (dev.isOpen()):
            dev.close()
        self.__dev = self
        if(self.__dev.isOpen() is False):
            self.__dev.open()

    def disconnect(self):
        self.__dev.close()

    def __del__(self):
        self.__dev.close()
