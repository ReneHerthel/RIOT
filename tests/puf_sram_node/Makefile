BOARD ?= nucleo-l152re

include ../Makefile.tests_common

USEMODULE += puf_sram

# NOTE: It is not possible to comment that out. Because this will lead to
#       compile errors.
USEMODULE += puf_sram_secret

USEMODULE += periph_eeprom
USEMODULE += shell

ifneq (,$(PUF_SRAM_GEN_HELPER))
	CFLAGS += -DPUF_SRAM_GEN_HELPER
#	ifneq (,$(filter nucleo-%,$(BOARD)))
		# on nucleo boards UART_DEV(0) interferes with the
		# on-board FTDI so change the STDIO bus for helper
		# generation
		#CFLAGS += -DSTDIO_UART_DEV=UART_DEV\(0\)
#	endif
endif

include $(RIOTBASE)/Makefile.include
