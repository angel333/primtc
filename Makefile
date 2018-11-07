.POSIX:

include config.mk

ARDCLI  = arduino-cli

SRCS    = $(wildcard *.c) $(wildcard *.cpp) $(wildcard *.ino)
PRG     = $(shell basename `pwd`)
HEX     = $(PRG).$(subst :,.,$(FQBN)).hex
ELF     = $(PRG).$(subst :,.,$(FQBN)).elf
ARDLIBS = DallasTemperature@3.8.0 \
				  hd44780@1.0.1 \
				  OneWire@2.3.4

all: deps $(HEX)

$(HEX): $(SRCS)
	$(ARDCLI) compile --fqbn $(FQBN) $(CURDIR)

upload:
	$(ARDCLI) upload --fqbn $(FQBN) -p $(PORT) $(CURDIR)

deps:
	@$(foreach lib,$(ARDLIBS),$(ARDCLI) lib install $(lib) || true; )

clean:
	$(RM) $(HEX) $(ELF)

.PHONY: all upload deps clean
