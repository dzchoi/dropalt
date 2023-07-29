# Usage: sudo python test.py

# Required:
#  - sudo pip install keyboard  # https://github.com/boppreh/keyboard#api
#  - sudo pip install pyserial  # https://pyserial.readthedocs.io/en/latest/pyserial_api.html

import keyboard
import random
from serial import Serial
import time



# Keycodes that are defined in hid_keycodes.hpp.
# Not used but can be used for testing only usb_thread::obj().hid_keyboard.report_press().
Keycode = {
    'NO':       b'\x00',
    'ROLLOVER': b'\x01',
    'POSTFAIL': b'\x02',
    'UNDEFINED': b'\x03',
    'A':        b'\x04',
    'B':        b'\x05',
    'C':        b'\x06',
    'D':        b'\x07',
    'E':        b'\x08',
    'F':        b'\x09',
    'G':        b'\x0a',
    'H':        b'\x0b',
    'I':        b'\x0c',
    'J':        b'\x0d',
    'K':        b'\x0e',
    'L':        b'\x0f',

    'M':        b'\x10',
    'N':        b'\x11',
    'O':        b'\x12',
    'P':        b'\x13',
    'Q':        b'\x14',
    'R':        b'\x15',
    'S':        b'\x16',
    'T':        b'\x17',
    'U':        b'\x18',
    'V':        b'\x19',
    'W':        b'\x1a',
    'X':        b'\x1b',
    'Y':        b'\x1c',
    'Z':        b'\x1d',
    '1':        b'\x1e',
    '2':        b'\x1f',

    '3':        b'\x20',
    '4':        b'\x21',
    '5':        b'\x22',
    '6':        b'\x23',
    '7':        b'\x24',
    '8':        b'\x25',
    '9':        b'\x26',
    '0':        b'\x27',
    'ENTER':    b'\x28',
    'ESC':      b'\x29',
    'BKSP':     b'\x2a',
    'TAB':      b'\x2b',
    'SPACE':    b'\x2c',
    '-':        b'\x2d',
    '=':        b'\x2e',
    '[':        b'\x2f',

    ']':        b'\x30',
    '\\':       b'\x31',
    'NONUS_HASH': b'\x32',  # Non-US # and ~ (Typically near the Enter key)
    ';':        b'\x33',  # ; (and :)
    "'":        b'\x34',  # ' and "
    '`':        b'\x35',  # Grave accent and tilde
    ',':        b'\x36',  # , and <
    '.':        b'\x37',  # . and >
    '/':        b'\x38',  # / and ?
    'CAPSLOCK': b'\x39',
    'F1':       b'\x3a',
    'F2':       b'\x3b',
    'F3':       b'\x3c',
    'F4':       b'\x3d',
    'F5':       b'\x3e',
    'F6':       b'\x3f',

    'F7':       b'\x40',
    'F8':       b'\x41',
    'F9':       b'\x42',
    'F10':      b'\x43',
    'F11':      b'\x44',
    'F12':      b'\x45',
    'PTRSCR':   b'\x46',
    'SCRLOCK':  b'\x47',
    'PAUSE':    b'\x48',
    'INSERT':   b'\x49',
    'HOME':     b'\x4a',
    'PGUP':     b'\x4b',
    'DELETE':   b'\x4c',
    'END':      b'\x4d',
    'PGDN':     b'\x4e',
    'RIGHT':    b'\x4f',

    'LEFT':     b'\x50',
    'DOWN':     b'\x51',
    'UP':       b'\x52',
    'NUMLOCK':  b'\x53',

    'LCTRL':    b'\xe0',
    'LSHIFT':   b'\xe1',
    'LALT':     b'\xe2',
    'LGUI':     b'\xe3',
    'RCTRL':    b'\xe4',
    'RSHIFT':   b'\xe5',
    'RALT':     b'\xe6',
    'RGUI':     b'\xe7',
}

Keymap = {  # encodes b'\x<row><col>'.
    'ESC':      b'\x00',
    '1':        b'\x01',
    '2':        b'\x02',
    '3':        b'\x03',
    '4':        b'\x04',
    '5':        b'\x05',
    '6':        b'\x06',
    '7':        b'\x07',
    '8':        b'\x08',
    '9':        b'\x09',
    '0':        b'\x0a',
    '-':        b'\x0b',
    '=':        b'\x0c',
    'BKSP':     b'\x0d',
    'DEL':      b'\x0e',

    'TAB':      b'\x10',
    'Q':        b'\x11',
    'W':        b'\x12',
    'E':        b'\x13',
    'R':        b'\x14',
    'T':        b'\x15',
    'Y':        b'\x16',
    'U':        b'\x17',
    'I':        b'\x18',
    'O':        b'\x19',
    'P':        b'\x1a',
    '[':        b'\x1b',
    ']':        b'\x1c',
    '\\':       b'\x1d',
    'HOME':     b'\x1e',

    'CAPSLOCK': b'\x20',
    'A':        b'\x21',
    'S':        b'\x22',
    'D':        b'\x23',
    'F':        b'\x24',
    'G':        b'\x25',
    'H':        b'\x26',
    'J':        b'\x27',
    'K':        b'\x28',
    'L':        b'\x29',
    ';':        b'\x2a',
    "'":        b'\x2b',
    'ENTER':    b'\x2d',
    'PGUP':     b'\x2e',

    'LSHIFT':   b'\x30',
    'Z':        b'\x32',
    'X':        b'\x33',
    'C':        b'\x34',
    'V':        b'\x35',
    'B':        b'\x36',
    'N':        b'\x37',
    'M':        b'\x38',
    ',':        b'\x39',
    '.':        b'\x3a',
    '/':        b'\x3b',
    'RSHIFT':   b'\x3c',
    'UP':       b'\x3d',
    'PGDN':     b'\x3e',

    'LCTRL':    b'\x40',
    'LGUI':     b'\x41',
    'LALT':     b'\x42',
    'SPACE':    b'\x46',
    'RALT':     b'\x4a',
    'FN':       b'\x4b',
    'LEFT':     b'\x4c',
    'DOWN':     b'\x4d',
    'RIGHT':    b'\x4e',
}



class Keyboard_emulator:
    def __init__(self, port: str):
        self.serial = Serial(port, 9600)
        # self.serial.reset_input_buffer()

        self.sent_events = []

        self.observed_events = []
        # Todo: `suppress=True` is not working on Linux.
        # (https://github.com/boppreh/keyboard/issues/22)
        keyboard.hook(lambda e: self.observed_events.append(e), suppress=True)

    def __del__(self):
        keyboard.unhook_all()

    def get_events(self, wait_secs: float =1) -> list[keyboard.KeyboardEvent]:
        # Retrieve gathered events from keyboard.hook().
        time.sleep(wait_secs)
        return self.observed_events

    @staticmethod
    def release(key_name: str) -> bytes:
        return Keymap[key_name] + b'\x00'

    @staticmethod
    def press(key_name: str) -> bytes:
        return Keymap[key_name] + b'\x01'

    def send_bytes(self, data: bytes):
        # '[}\x0f' is the magic word to start emulator.
        assert len(data) < 256
        self.serial.write(b'[}\x0f' + len(data).to_bytes(1, 'little') + data)

    def send_events(self, events: str|list[str]):
        # Send the given events over serial. The events can be either a string or a list.
        if isinstance(events, str):
            events = [e.strip() for e in events.split(',')]

        self.sent_events += events
        byte_events = bytes()

        for e in events:
            key_name, event_type = e.split()
            if event_type == "down":
                byte_events += self.press(key_name)
            else:
                byte_events += self.release(key_name)

        self.send_bytes(byte_events)

    def build_expected_events(self, events: None|str|list[str] =None) -> list[list[str]]:
        # Build expected events from the given events or from the events that have been
        # sent using send_events(), based on the Low-latency key registering algorithm in
        # usbus_hid_keyboard.cpp.

        if events is None:
            events = self.sent_events
        elif isinstance(events, str):
            events = [e.strip() for e in events.split(',')]

        expected_events = []
        last_key_press = None

        for e in events:
            key_name, event_type = e.split()

            if event_type == "down":
                expected_events.append([e])
                last_key_press = key_name
            elif key_name == last_key_press:
                expected_events.append([e])
                last_key_press = None
            else:
                expected_events[-1].append(e)

        return expected_events

    def verify(self, expected_events: None|str|list[list[str]] =None):
        # Verify that the device behaves as expected according to the sent events.

        # Example of str expected_events: "A down, B down|A up, C down, C up|B up"
        #  where "<event_1>|<event_2>|..." means those events can come in any order.

        def parse(events: str) -> list[list[str]]:
            # Will return [["A down"], ["B down", "A up"], ["C down"], ["C up", "B up"]].
            return [[e.strip() for e in pairs.split('|')]
                        for pairs in events.split(',')]

        if expected_events is None:
            expected_events = self.build_expected_events()
        elif isinstance(expected_events, str):
            expected_events = parse(expected_events)

        for actual_event in self.get_events():
            actual_event = ' '.join((actual_event.name.upper(), actual_event.event_type))

            assert expected_events, "More events observed than expected"
            assert actual_event in expected_events[0],\
                f"'{actual_event}' not expected in {expected_events[0]}\n" +\
                f"sent_events: {self.sent_events}"

            expected_events[0].remove(actual_event)
            if not expected_events[0]:
                expected_events.pop(0)

        assert not expected_events, "Less events observed than expected"

        # Prepare for next test
        print(" ok")
        self.sent_events.clear()
        self.observed_events.clear()



# Test cases
    def test_AAA(self):
        self.send_events("A down, A up, A down, A up, A down, A up")
        self.verify()

    def test_ABC(self):
        self.send_events("A down, B down, A up, C down, C up, B up")
        self.verify()

    def test_9f(self):
        self.send_events("9 down, F down, 9 up, F up")
        self.verify()

    def test_SHIFT_90(self):
        # Double tap_holds
        self.send_events("SPACE down, 9 down")
        time.sleep(0.199)
        self.send_events("0 down, 9 up, 0 up, SPACE up")
        self.verify("SHIFT down, 9 down, 0 down| 9 up, 0 up| SHIFT up")

    def test_random(self, keys_to_test: str):
        # Random test from the given keys_to_test, which is a string that contains
        # (possibly duplicate) one-letter key names.

        events_remaining = [key_name + " down" for key_name in keys_to_test]
        events_to_send: list[str] = []
        already_pressed: list[str] = []

        while events_remaining:
            e = random.choice(events_remaining)
            key_name, event_type = e.split()
            if event_type == "down" and key_name in already_pressed:
                continue

            events_remaining.remove(e)
            events_to_send.append(e)

            if event_type == "down":
                events_remaining.append(key_name + " up")
                already_pressed.append(key_name)
            else:
                already_pressed.remove(key_name)

        self.send_events(events_to_send)
        self.verify()



def repeat(test, count=10):
    # Repeat an existing test.
    for i in range(0, count):
        test()



device = Keyboard_emulator("/dev/ttyACM1")
device.test_AAA()
device.test_ABC()
device.test_9f()
repeat(lambda: device.test_SHIFT_90())  # Or repeat(device.test_SHIFT_90)

# stress test for NKRO
device.test_random("AAAAAAAAAA,BBBBBBBBBB,1111111111")
device.test_random("ABCDEFGHIJ,KLMNOPQRST,1234567890,1234567890")
