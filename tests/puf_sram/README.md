# Tool Introduction
This tool is designed to interface with a test to get the outputs of the PUF SRAM feature. In order to run the test a certain hardware setup is required due to the requirement to power off the DUT (device under test) for a certain time. Furthermore, the module detects button and/or software resets. If you push the reset button for example (without powering off the DUT), a warning should be printed to the console.

# Setup
## Required Tools
- DUT (a supported RIOT target board)
- USB to UART converter that supports setting the RTS pin and 5 volts (ie. FT232RL FTDI USB to TTL adapter)
- MOSFET to control power to the DUT (ie. STP36NF06L)
- Jumper cables
- Solderless breadboard

## Wiring Example
1. RTS <--> MOSFET gate pin (FT232RL RTS - STP36NF06L 1)
2. +5V <--> MOSFET drain pin (FT232RL 5V - STP36NF06L 2)
3. DUT Power <--> MOSFET source pin (E15 - STP36NF06L 3)
4. DUT UART TX <--> USB to UART RX
5. GND <--> GND

## Example Setup
![PUF SRAM test setup](https://raw.githubusercontent.com/wiki/RIOT-OS/RIOT/images/puf-sram-setup.jpg)


# Running random seed test
1. Program the DUT with the puf_sram test (you can remove the line `USEMODULE += puf_sram_secret` from the Makefile if you are only interested in random seeds)
2. Plug the USB to UART converter in (it should be done first so it can autoconnect to the serial port)
3. Connect all wires
4. If necessary for your board (almost all Nucleos) change jumpers to only run on power provided by the USB to USRT converter
5. Run the example_test.py

# Running secret ID test
## Generate helper data
1. Program DUT with measurement firmware. There is a build tool that should do everything for you. Type `BOARD=<your board> make flash-puf-helpergen`
2. Plug the USB to UART converter in (please also read [Special Notes](#special-notes) below)
3. Start helper data generation via `make puf-helpergen`

## Running test
4. Follow instructions from [Running random seed test](#runing-random-seed-test)

# Running Custom Tests
Different test configurations can be executed. Run `example_test.py -h` for an overview of
configurable parameters

# Special Notes
In order to externally re-power boards we rely on external FTDI hardware to
control the power switching and handle the UART connection over USB.

STM32 Nucleo boards provide a debug chip that is connected to the MCUs first
UART bus. This bus is used for STDIO in RIOT by default.

When powering a Nucleo baord externally (e.g., via E5V) the debug chip is
powered as well. It automatically pulls up the TX line which makes it
impossible to use for data transmission with the external FTDI in parallel.

To write back the helper data to EEPROM during [Generate helper data][##generate-helper-data]
(step 3.) we rely on a proper TX line. So we changed
the STDIO device to `UART_DEV(1)` for all Nucelo boards when generating helper
data. This requires re-plugging!
