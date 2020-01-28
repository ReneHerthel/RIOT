BOARD ?= iotlab-m3

include ../Makefile.tests_common

USEMODULE += puf_sram
USEMODULE += puf_sram_secret
USEMODULE += shell

ifneq (,$(filter nucleo-%,$(BOARD)))
	USEMODULE += periph_eeprom
	CFLAGS += -DUSE_MEM_EEPROM=1
endif

ifneq (,$(filter iotlab-m3,$(BOARD)))
	# TODO: USEMODULE += n25q128a
	CFLAGS += -DUSE_MEM_N25Q128A=1
endif

#ifneq (,$(PUF_SRAM_GEN_HELPER))
#	CFLAGS += -DPUF_SRAM_GEN_HELPER
#	ifneq (,$(filter nucleo-%,$(BOARD)))
		# on nucleo boards UART_DEV(0) interferes with the
		# on-board FTDI so change the STDIO bus for helper
		# generation
		#CFLAGS += -DSTDIO_UART_DEV=UART_DEV\(0\)
#	endif
#endif

#ifneq (,$(filter iotlab-m3,$(BOARD)))
#	CFLAGS += -DUSE_IOTLABM3_BOARD=1
#else ifneq (,$(filter nucleo-%,$(BOARD)))
#	USEMODULE += periph_eeprom
#endif

include $(RIOTBASE)/Makefile.include
