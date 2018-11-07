# This is my original version written for Raspberry Pi. My Pi died so I
# rewrote it for Arduino...
#
# Install:
# aptitude install python-smbus (ale uz je)
# sudo pip install RPLCD
# sudo pip install w1thermsensor
# 
# rasp-config: enable i2c a 1wire

import time
from threading import Thread
from RPi import GPIO
from w1thermsensor import W1ThermSensor
from RPLCD.i2c import CharLCD

# Measuring temperature is in another thread and always available in the current_temp variable.
current_temp = 0
def temp_sensor_thread():
    global current_temp
    global temp_sensor
    temp_sensor = W1ThermSensor()
    while True:
        current_temp = temp_sensor.get_temperature()
temp_sensor_thread = Thread(target=temp_sensor_thread)
temp_sensor_thread.start()

GPIO.setmode(GPIO.BCM)

RELAY_VALVE_LEFT = 24
RELAY_VALVE_RIGHT = 25
LIMIT_SWITCH_LEFT = 8
LIMIT_SWITCH_RIGHT = 7

GPIO.setup(RELAY_VALVE_LEFT, GPIO.OUT, initial=0)
GPIO.setup(RELAY_VALVE_RIGHT, GPIO.OUT, initial=0)
GPIO.setup(LIMIT_SWITCH_LEFT, GPIO.IN, pull_up_down=GPIO.PUD_UP)
GPIO.setup(LIMIT_SWITCH_RIGHT, GPIO.IN, pull_up_down=GPIO.PUD_UP)

lcd = CharLCD('PCF8574', 0x27)

target_temp = 60
pulse_length_sec = 2
pause_sec = 2*60
treshold_diff = 2

INTENT_DO_NOTHING = 0
INTENT_MOVE_LEFT = 1
INTENT_MOVE_RIGHT = 2

STATE_WAITING = 0
STATE_MOVING = 1

state = STATE_WAITING
next_switch_ts = time.time() + pause_sec

try:
    while True:

        # Switch states
        remaining = next_switch_ts - time.time()
        if remaining <= 0:
            if state == STATE_MOVING or (state == STATE_WAITING and intent == INTENT_DO_NOTHING):
                next_switch_ts = time.time() + pause_sec
                state = STATE_WAITING
            elif state == STATE_WAITING:
                next_switch_ts = time.time() + pulse_length_sec
                state = STATE_MOVING
            continue

        # Read limit switches, temperature, ...
        left_limit = True if GPIO.input(LIMIT_SWITCH_LEFT) == 0 else False
        right_limit = True if GPIO.input(LIMIT_SWITCH_RIGHT) == 0 else False

        # In WAITING state relays are stopped and intent is determined
        if state == STATE_WAITING:
            GPIO.output(RELAY_VALVE_LEFT, 0)
            GPIO.output(RELAY_VALVE_RIGHT, 0)
            # Determine intent
            if current_temp > (target_temp + treshold_diff):
                intent = INTENT_MOVE_LEFT
            elif current_temp < (target_temp - treshold_diff):
                intent = INTENT_MOVE_RIGHT
            else:
                intent = INTENT_DO_NOTHING
        # .. In MOVING state relays are moved if limit switches are ok
        elif state == STATE_MOVING:
            if intent == INTENT_MOVE_LEFT and left_limit == False:
                GPIO.output(RELAY_VALVE_LEFT, 1)
                GPIO.output(RELAY_VALVE_RIGHT, 0)
            elif intent == INTENT_MOVE_RIGHT and right_limit == False:
                GPIO.output(RELAY_VALVE_LEFT, 0)
                GPIO.output(RELAY_VALVE_RIGHT, 1)
            else: # limits are triggered
                GPIO.output(RELAY_VALVE_LEFT, 0)
                GPIO.output(RELAY_VALVE_RIGHT, 0)
            continue # don't use display here

        print(GPIO.input(LIMIT_SWITCH_LEFT))
        print(GPIO.input(LIMIT_SWITCH_RIGHT))
        # print temps
        lcd.cursor_pos = (0,0)
        lcd.write_string('{:12}[{:^6}]\n\r'.format('Mode:', 'Auto'))
        lcd.write_string('Temp: {:>4.1f}{:1} -> {:>4.1f}{:1}\n\r'.format(current_temp, 'C', target_temp, 'C'))
        lcd.write_string('{:2} {:3} [{:3.0f}s] {:3} {:2}'.format(
            'XX' if left_limit else 'OK',
            '<==' if intent == INTENT_MOVE_LEFT else '===',
            remaining if state == STATE_WAITING else 0,
            '==>' if intent == INTENT_MOVE_RIGHT else '===',
            ('XX' if remaining % 1 < .5 else '  ') if right_limit else 'OK',
            ))
        lcd.write_string('{:^20}\n\r'.format('Temp+'))

        time.sleep(.05)

except KeyboardInterrupt:
    GPIO.cleanup()

