# Usage: python test.py

# Required:
#  - pip install pyserial  # https://pyserial.readthedocs.io/en/latest/pyserial_api.html

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
    'a':        b'\x04',
    'b':        b'\x05',
    'c':        b'\x06',
    'd':        b'\x07',
    'e':        b'\x08',
    'f':        b'\x09',
    'g':        b'\x0a',
    'h':        b'\x0b',
    'i':        b'\x0c',
    'j':        b'\x0d',
    'k':        b'\x0e',
    'l':        b'\x0f',

    'm':        b'\x10',
    'n':        b'\x11',
    'o':        b'\x12',
    'p':        b'\x13',
    'q':        b'\x14',
    'r':        b'\x15',
    's':        b'\x16',
    't':        b'\x17',
    'v':        b'\x18',
    'v':        b'\x19',
    'w':        b'\x1a',
    'x':        b'\x1b',
    'y':        b'\x1c',
    'z':        b'\x1d',
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

Keymap = {  # encodes the slot index of each key.
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

    'TAB':      b'\x0f',
    'q':        b'\x10',
    'w':        b'\x11',
    'e':        b'\x12',
    'r':        b'\x13',
    't':        b'\x14',
    'y':        b'\x15',
    'u':        b'\x16',
    'i':        b'\x17',
    'o':        b'\x18',
    'p':        b'\x19',
    '[':        b'\x1a',
    ']':        b'\x1b',
    '\\':       b'\x1c',
    'HOME':     b'\x1d',

    'CAPSLOCK': b'\x1e',
    'a':        b'\x1f',
    's':        b'\x20',
    'd':        b'\x21',
    'f':        b'\x22',
    'g':        b'\x23',
    'h':        b'\x24',
    'j':        b'\x25',
    'k':        b'\x26',
    'l':        b'\x27',
    ';':        b'\x28',
    "'":        b'\x29',
    'ENTER':    b'\x2b',  # +2
    'PGUP':     b'\x2c',

    'LSHIFT':   b'\x2d',
    'z':        b'\x2f',  # +2
    'x':        b'\x30',
    'c':        b'\x31',
    'v':        b'\x32',
    'b':        b'\x33',
    'n':        b'\x34',
    'm':        b'\x35',
    ',':        b'\x36',
    '.':        b'\x37',
    '/':        b'\x38',
    'RSHIFT':   b'\x39',
    'UP':       b'\x3a',
    'PGDN':     b'\x3b',

    'LCTRL':    b'\x3c',
    'LGUI':     b'\x3d',
    'LALT':     b'\x3e',
    'SPACE':    b'\x42',  # +4
    'RALT':     b'\x46',  # +4
    'FN':       b'\x47',
    'LEFT':     b'\x48',
    'DOWN':     b'\x49',
    'RIGHT':    b'\x4a',  # == 74
}



class Keyboard_emulator:
    def __init__(self, port: str):
        self.serial = Serial(port, 9600)
        # self.serial.reset_input_buffer()

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

        byte_events = bytes()

        for e in events:
            key_name, event_type = e.split()
            if event_type == "down":
                byte_events += self.press(key_name)
            else:
                byte_events += self.release(key_name)

        self.send_bytes(byte_events)

    def verify(self, expected_text: str):
        # Verify that the device behaves as expected according to the sent events.
        time.sleep(1)
        print(" ok", end='')
        self.send_events("ENTER down, ENTER up")
        actual_text = input()
        assert expected_text == actual_text,\
            f"Not OK. \"{expected_text}\" != \"{actual_text}\""



# Test cases
    def test_aaa(self):
        self.send_events("a down, a up, a down, a up, a down, a up")
        self.verify("aaa")

    def test_abc(self):
        self.send_events("a down, b down, a up, c down, c up, b up")
        self.verify("abc")

    def test_9f(self):
        self.send_events("9 down, f down, 9 up, f up")
        self.verify("9f")

    def test_shift90(self):
        # Double tap_holds
        release = ['9 up', '0 up', 'SPACE up']
        random.shuffle(release)
        delay = 0.21 if release[0].startswith('SPACE') else random.uniform(0.1, 0.3)

        self.send_events(['SPACE down', '9 down'])
        time.sleep(delay)
        self.send_events(['0 down'] + release)
        self.verify("()")

    def test_shift7890(self):
        # Double tap_holds
        release = ['7 up', '8 up', '9 up', '0 up']
        random.shuffle(release)
        release.append('SPACE up')

        self.send_events("SPACE down, 7 down, 8 down, 9 down, 0 down")
        self.send_events(release)
        self.verify("&*()")

    def test_random(self, keys_to_test: str):
        # Random test from the given keys_to_test, which is a string that contains
        # (possibly duplicate) one-letter key names.

        events_remaining = [key_name + " down" for key_name in keys_to_test]
        events_to_send: list[str] = []
        already_pressed: list[str] = []
        expected_text = ""

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
                expected_text += key_name
            else:
                already_pressed.remove(key_name)

        self.send_events(events_to_send)
        self.verify(expected_text)



def repeat(test, count=10):
    # Repeat an existing test.
    for i in range(0, count):
        test()



device = Keyboard_emulator("/dev/ttyACM2")
device.test_aaa()
device.test_abc()
device.test_9f()

# stress test for NKRO
device.test_random("1234567890")
device.test_random("aaaaaaaaaa,bbbbbbbbbb,1111111111")
device.test_random("abcdefghij,klmnopqrst,1234567890,1234567890")

repeat(device.test_shift90)  # Or repeat(lambda: device.test_shift90())
repeat(device.test_shift7890)
