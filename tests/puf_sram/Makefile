BOARD ?= nucleo-l073rz

include ../Makefile.tests_common

DISABLE_MODULE += test_utils_interactive_sync

USEMODULE += puf_sram
USEMODULE += puf_sram_secret

USEMODULE += periph_eeprom
USEMODULE += shell

ifneq (,$(PUF_SRAM_GEN_HELPER))
	CFLAGS=-DPUF_SRAM_GEN_HELPER
	ifneq (,$(filter nucleo-%,$(BOARD)))
		# on nucleo boards UART_DEV(0) interferes with the
		# on-board FTDI so change the STDIO bus for helper
		# generation
		CFLAGS += -DSTDIO_UART_DEV=UART_DEV\(1\)
	endif
endif
>>>>>>> SPLITME

include $(RIOTBASE)/Makefile.include
