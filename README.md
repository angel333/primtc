# PrimTC - Primitive Temperature Control

Just a simple temperature controller for a three-way valve, written in
Arduino.


## Requirements

- [hd44780](//github.com/duinoWitchery/hd44780)
- [DallasTemperature](//github.com/milesburton/Arduino-Temperature-Control-Library)
- [OneWire](//github.com/PaulStoffregen/OneWire)


## Install

Just open the sketch in Arduino IDE, install all dependencies through
the Library Manager and upload the sketch to your MCU.


### Using the Makefile

An alternative to Arduino IDE is to use the included Makefile (that uses
[arduino-cli]):

1. Setup `arduino-cli`:

   ```
   $ go get github.com/arduino/arduino-cli
   $ arduino-cli core update-index
   $ arduino-cli core install arduino:avr
   $ make
   ```

   > Note: Make sure `arduino-cli` is in your `$PATH`.

2. Modify the `FQBN` and `PORT` variables in `config.mk` to match your
   MCU and port. See [arduino-cli's README] for details.

   ```
   FQBN = arduino:avr:nano:cpu=atmega328old
   PORT = /dev/ttyUSB0
   ```

3. Install dependencies, build and upload the sketch:

   ```
   $ make deps
   $ make
   $ make upload
   ```

[arduino-cli]: https://github.com/arduino/arduino-cli
[arduino-cli's README]: https://github.com/arduino/arduino-cli/blob/master/README.md


## License

Because of dependencies on copylefted libraries:

- the whole project is licensed under GPLv3;
- parts written by me are dual-licensed under GPLv3 and MIT.
