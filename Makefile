ARDCLI = arduino-cli
FQBN   = arduino:avr:nano:cpu=atmega328old
PORT   = /dev/ttyUSB0

SRCS   = $(wildcard *.c) $(wildcard *.cpp) $(wildcard *.ino)
PRG    = $(shell basename `pwd`)
HEX    = $(PRG).$(subst :,.,$(FQBN)).hex
ELF    = $(PRG).$(subst :,.,$(FQBN)).elf

$(HEX): $(SRCS)
	$(ARDCLI) compile --fqbn $(FQBN) $(CURDIR)

upload:
	$(ARDCLI) upload --fqbn $(FQBN) -p $(PORT) $(CURDIR)

clean:
	$(RM) $(HEX) $(ELF)

.PHONY: upload clean
