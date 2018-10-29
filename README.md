# PrimTC - Primitive Temperature Control

Just a simple temperature controller for a three-way valve, written in
Arduino.

## Requirements

- [duinoWitchery/hd44780](//github.com/duinoWitchery/hd44780)
- [milesburton/Arduino-Temperature-Control-Library](//github.com/milesburton/Arduino-Temperature-Control-Library)
- [PaulStoffregen/OneWire](//github.com/PaulStoffregen/OneWire)

## Install

Just open the sketch in Arduino IDE, install all dependencies through
the Library Manager and upload the sketch to your MCU.

### Using the Makefile

An alternative to Arduino IDE is to use the included Makefile:

1. Install all requirements with Library Manager in Arduino IDE.

2. Setup `arduino-cli`:

   ```
   $ go get github.com/arduino/arduino-cli
   $ arduino-cli core update-index
   ```

[arduino-cli's README]: https://github.com/arduino/arduino-cli/blob/master/README.md

3. Modify the `FQBN` and `PORT` variables in `Makefile` to match your
   MCU and port. See [arduino-cli's README] for details.

   ```
   FQBN   = arduino:avr:nano:cpu=atmega328old
   PORT   = /dev/ttyUSB0
   ```

4. Upload the sketch:

   ```
   $ make && make upload
   ```

## License

See the specific files for details, but in general:

- the whole project is licensed under GPLv3;
- parts written by me are dual-licensed under GPLv3 and MIT.
