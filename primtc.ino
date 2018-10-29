/* SPDX-License-Identifier: MIT OR GPL-3.0-only
 *
 * ---------------------------------------------------------------------
 *
 * Copyright (c) 2018 Ondrej Simek
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---------------------------------------------------------------------
 *
 * MIT License
 * 
 * Copyright (c) 2018 Ondrej Simek
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>


// -- Settings ---------------------------------------------------------

/*
 * Temperature settings
 *
 * - Default target temperature
 * - Target tolerance: temperature difference to be tolerated before
 *   action is taken
 *
 * ! both settings are in milli-Cs - i.e. 600 = 60 degrees C !
 */

#define DEFAULT_TARGET_TEMP 600
#define TARGET_TEMP_TOLERANCE 20


/*
 * Pin settings
 */

#define PIN_ONEWIRE             A0
#define PIN_VALVE_RELAY_LEFT    5
#define PIN_VALVE_RELAY_RIGHT   6
#define PIN_LIMIT_SWITCH_LEFT   7
#define PIN_LIMIT_SWITCH_RIGHT  8
#define PIN_BUTTON_1            3


/*
 * Temperature sensor settings
 *
 * ! Higher precision => slower conversion times !
 *
 * My DS18S20's conversion times seem to follow this formula:
 *
 *   20 + 660 * (2 ^ (12 - precision)) 
 *
 * Resolution:      Conv time:
 *  9bits (.50)   = 102.5ms
 * 10bits (.25)   = 185.0ms
 * 11bits (.125)  = 350.0ms
 * 12bits (.0625) = 680.0ms
 *
 * IGNORE_BELOW and IGNORE_ABOVE will trigger an error state.
 * ! both settings are in milli-Cs - i.e. 2000 = 200 degrees C !
 */

#define TEMP_SENSOR_PRECISION 11
#define TEMP_SENSOR_IGNORE_BELOW 0
#define TEMP_SENSOR_IGNORE_ABOVE 2000


/*
 * Phase duration settings
 *
 * These settings determine how long will the system wait (decision
 * phase) before it takes action (action phase) and how long this action
 * takes.
 */

#define PHASE_DURATION_DECISION_MS 120000
#define PHASE_DURATION_ACTION_MS 2000

/*
 * Debounce delay for button
 */

#define BUTTON_DEBOUNCE_DELAY 250

/*
 * One button settings
 */

#define ONE_BUTTON_TEMP_MIN 300
#define ONE_BUTTON_TEMP_MAX 900
#define ONE_BUTTON_TEMP_STEP 25

// ---------------------------------------------------------------------


// Leaving some 5ms extra...
#if 9 == TEMP_SENSOR_PRECISION
#define TEMP_SENSOR_WAIT_TIME 108
#elif 10 == TEMP_SENSOR_PRECISION
#define TEMP_SENSOR_WAIT_TIME 190
#elif 11 == TEMP_SENSOR_PRECISION
#define TEMP_SENSOR_WAIT_TIME 355
#elif 12 == TEMP_SENSOR_PRECISION
#define TEMP_SENSOR_WAIT_TIME 685
#endif


hd44780_I2Cexp lcd;

OneWire one_wire(PIN_ONEWIRE);
DallasTemperature sensors(&one_wire);
byte temp_sensor_addr[8];

char main_screen[81] = "------ PrimTC ------"
                       "Current:    Target: "
                       "[ --.- ] -- [ --.- ]"
                       "OK === [----] === OK";

uint16_t temp_current_mc; // milli-celsius
uint16_t temp_target_mc;  // milli-celsius

uint32_t next_temp_update = 0;
uint32_t next_phase_switch;

typedef enum { DECISION = 0, ACTION } phase_t;
phase_t current_phase;

typedef enum { NO_MOVEMENT = 0, MOVE_LEFT, MOVE_RIGHT } intent_t;
intent_t intent;

// for debouncing
uint32_t button_last_press[1] = { 0 };

bool temp_sensor_err = 0;

// Utility functions (see at the bottom)
void fp2str(char*, uint32_t, const uint8_t, const int8_t);

bool inline left_limit (void)
{
    return LOW == digitalRead(PIN_LIMIT_SWITCH_LEFT);
}

bool inline right_limit (void)
{
    return LOW == digitalRead(PIN_LIMIT_SWITCH_RIGHT);
}


// -- Display functions ------------------------------------------------

void inline lcd_update_current_temp (uint16_t temp)
{
    fp2str(main_screen+(20*2)+2, temp, 4, 1);
}

void inline lcd_update_phase (uint8_t phase)
{
    char *pos = main_screen + (20*2)+7;
    if (temp_sensor_err) {
        if (millis() % 2000 < 500) {
            strncpy(pos, "] EE [", 6);
        } else if (millis() % 2000 < 1500) {
            strncpy(pos, "]!EE![", 6);
        } else {
            strncpy(pos, "U MAD?", 6);
        }
        // here's a more civil version of the above...
        // strncpy(pos, millis() % 1000 < 500 ? "!  !" : " EE ", 4);
        return;
    }
    strncpy(pos, ACTION == phase ? "] !! [" : "] => [", 6);
}

void inline lcd_update_intent (void)
{
    memset(main_screen+(20*3)+3, MOVE_LEFT == intent ? '<' : '=', 3);
    memset(main_screen+(20*3)+14, MOVE_RIGHT == intent ? '>' : '=', 3);
}

void inline lcd_update_limits (void)
{
    strncpy(main_screen+(20*3), left_limit() ? "XX " : "OK ", 3);
    strncpy(main_screen+(20*3)+17, right_limit() ? " XX" : " OK", 3);
    // both limit switches
    if (left_limit() && right_limit()) {
        strncpy(main_screen+(20*3), "WTF", 3);
        strncpy(main_screen+(20*3)+17, "WTF", 3);
    }
}

void update_display()
{
    fp2str(main_screen+(20*2)+14, temp_target_mc, 4, 1);

    fp2str(
        main_screen+(20*3)+8,
        (next_phase_switch - millis()) / 1000,
        4,
        -1);

    lcd_update_intent();
    lcd_update_limits();

    // this could be in switch_phase() if there wasn't the error state
    lcd_update_phase(current_phase);

    lcd.home();
    lcd.write(main_screen, 20);
    lcd.write(main_screen+40, 20); // hd44780 lib flips lines 1 and 2
    lcd.write(main_screen+20, 20);
    lcd.write(main_screen+60, 20);
}

// ---------------------------------------------------------------------


void
init_sensors (void)
{
    sensors.begin();
    sensors.getAddress(temp_sensor_addr, 0);
    sensors.setWaitForConversion(false);
    sensors.setResolution(TEMP_SENSOR_PRECISION);

    // Sensors seem to return wrong value initially.
    // ... waiting some time is no big deal though...
    delay(TEMP_SENSOR_WAIT_TIME);
}


void
init_lcd (void)
{
	lcd.begin(20,4);
}


void
switch_phase (uint8_t phase)
{
    current_phase = phase;

    next_phase_switch = millis()
        + (phase
            ? PHASE_DURATION_ACTION_MS
            : PHASE_DURATION_DECISION_MS);
}


void
auto_switch_phase (void)
{
    if (next_phase_switch > millis()) return;

    if (NO_MOVEMENT == intent)    return switch_phase(DECISION);
    if (ACTION == current_phase)  return switch_phase(DECISION);

    switch_phase(ACTION);
}


void
init_pins (void)
{
    pinMode(PIN_VALVE_RELAY_LEFT, OUTPUT);
    pinMode(PIN_VALVE_RELAY_RIGHT, OUTPUT);
    pinMode(PIN_LIMIT_SWITCH_LEFT, INPUT_PULLUP);
    pinMode(PIN_LIMIT_SWITCH_RIGHT, INPUT_PULLUP);
    pinMode(PIN_BUTTON_1, INPUT_PULLUP);
}


void
switch_temp (void)
{
    if (temp_target_mc >= ONE_BUTTON_TEMP_MAX) {
        temp_target_mc = ONE_BUTTON_TEMP_MIN;
        return;
    }
    temp_target_mc += ONE_BUTTON_TEMP_STEP;
}


void
setup()
{
    init_sensors();
    init_lcd();
    init_pins();

    temp_target_mc = DEFAULT_TARGET_TEMP;
    intent = NO_MOVEMENT;

    switch_phase(0);

    // Serial.begin(19200);
}


void
exec_intent (void)
{
    if (temp_sensor_err) return switch_phase(0);

    if (MOVE_LEFT == intent) {
        if (left_limit()) return switch_phase(0);
        digitalWrite(PIN_VALVE_RELAY_LEFT, HIGH);
        digitalWrite(PIN_VALVE_RELAY_RIGHT, LOW);
        return;
    }

    if (MOVE_RIGHT == intent) {
        if (right_limit()) return switch_phase(0);
        digitalWrite(PIN_VALVE_RELAY_LEFT, LOW);
        digitalWrite(PIN_VALVE_RELAY_RIGHT, HIGH);
    }
}


bool
button_pressed (uint8_t id)
{
    if (digitalRead(PIN_BUTTON_1) == HIGH)
        return 0;
    if ((button_last_press[0] + BUTTON_DEBOUNCE_DELAY) > millis())
        return 0;

    button_last_press[0] = millis();
    return 1;
}


void
make_intent (void)
{
    if (
        temp_current_mc < TEMP_SENSOR_IGNORE_BELOW ||
        temp_current_mc > TEMP_SENSOR_IGNORE_ABOVE
    ) {
        intent = NO_MOVEMENT;
        temp_sensor_err = 1;
        return;
    }

    temp_sensor_err = 0;

    if (temp_current_mc < (temp_target_mc - TARGET_TEMP_TOLERANCE)) {
        intent = MOVE_RIGHT;
        return;
    }

    if (temp_current_mc > (temp_target_mc + TARGET_TEMP_TOLERANCE)) {
        intent = MOVE_LEFT;
        return;
    }

    intent = NO_MOVEMENT;
}


void
loop()
{
    auto_switch_phase();

    // Decision phase
    if (current_phase == DECISION)
    {
        digitalWrite(PIN_VALVE_RELAY_LEFT, LOW);
        digitalWrite(PIN_VALVE_RELAY_RIGHT, LOW);
        make_intent();
    } else {
        exec_intent();
    }

    // Buttons
    if (button_pressed(0)) switch_temp();

    // Update current temperature
    if (next_temp_update < millis()) {
        sensors.requestTemperaturesByAddress(temp_sensor_addr);
        next_temp_update = millis() + TEMP_SENSOR_WAIT_TIME;
        temp_current_mc = sensors.getTempC(temp_sensor_addr) * 10;
        lcd_update_current_temp(temp_current_mc);
    }

    update_display();
}


// -- Utility functions ------------------------------------------------


/*
 * Converting integers to strings
 *
 * Examples:
 *
 *   fp2str(buffer, 123, 4, 1);    => "12.3"
 *   fp2str(buffer, 123, 4, 2);    => "1.23"
 *   fp2str(buffer, 123, 4, -1);   => " 123"
 */

void
fp2str(
	char *buffer,
	uint32_t num,
	const uint8_t buff_size,
	const int8_t decimals)
{
	memset(buffer, ' ', buff_size);

    // position of '.'
	int8_t dec_pos = buff_size - decimals - 1;

	char pos;
	for (pos = buff_size - 1; pos != -1; pos--) {
		if (pos == dec_pos) continue;           // step over '.'
		if (num == 0 && pos < dec_pos-1) break; // only one 0 before '.'
		buffer[pos] = (num % 10) + 0x30;
		num /= 10;
	}

	// number did not fit
	if (num != 0) {
		memset(buffer, '-', buff_size);
	}

	if (dec_pos < buff_size) buffer[dec_pos] = '.';
}


// vim: et:ts=4:sw=4
