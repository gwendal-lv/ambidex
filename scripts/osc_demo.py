#!/usr/bin/env python3
"""Ambidex OSC demo: morphs from A to B, changes preset A, then morphs back to A.

Hold a few notes (or play) while it runs, to hear the morphing. See doc/OSC.md
for the OSC API.

Requires python-osc, e.g. in a venv (see README):

    .venv/bin/python scripts/osc_demo.py [--host 192.168.1.20] [--port 9000] [--preset 12]
"""

import argparse
import random
import socket
import time

from pythonosc.osc_message import OscMessage
from pythonosc.osc_message_builder import OscMessageBuilder


class AmbidexError(Exception):
    """An error reported by Ambidex (/ambidex/error)."""


class Ambidex:
    """A minimal Ambidex OSC client: one UDP socket, for the commands and their replies."""

    def __init__(self, host="127.0.0.1", port=9000, timeout=1.0):
        self.target = (host, port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(timeout)

    def send(self, address, *args):
        builder = OscMessageBuilder(address)
        for arg in args:
            builder.add_arg(arg)
        self.sock.sendto(builder.build().dgram, self.target)

    def request(self, address, reply_address):
        """Sends a query, and returns the arguments of its reply.

        Raises AmbidexError if a previous command failed (its error comes before the reply).
        """
        self.send(address)
        errors = []
        while True:
            try:
                data, _ = self.sock.recvfrom(65536)
            except socket.timeout:
                raise TimeoutError(f"no reply from Ambidex at {self.target[0]}:{self.target[1]}") from None
            message = OscMessage(data)
            if message.address == "/ambidex/error":
                errors.append(message.params[0])
            elif message.address == reply_address:
                # (raised once the reply is read, so that it isn't taken for the next one's)
                if errors:
                    raise AmbidexError("; ".join(errors))
                return message.params

    def presets(self):
        """Names of the library presets; a preset's index is its position in this list."""
        return self.request("/ambidex/presets/list", "/ambidex/presets")

    def state(self):
        index_a, name_a, index_b, name_b, morph = self.request("/ambidex/state", "/ambidex/state")
        return {"a": (index_a, name_a), "b": (index_b, name_b), "morph": morph}

    def set_preset(self, slot, index):
        """Loads a library preset into slot "a" or "b" (discards its unsaved edits)."""
        self.send(f"/ambidex/slot/{slot}/preset", int(index))

    def set_morph(self, value):
        """0 = only A is heard, 1 = only B."""
        self.send("/ambidex/morph", float(value))

    def morph_to(self, target, seconds, rate=50):
        """Moves the morph smoothly from its current position to the target."""
        start = self.state()["morph"]
        steps = max(1, round(seconds * rate))
        for step in range(1, steps + 1):
            t = step / steps
            eased = t * t * (3 - 2 * t)  # (smoothstep: starts and ends slowly)
            self.set_morph(start + (target - start) * eased)
            time.sleep(seconds / steps)


def describe(state):
    def slot(name):
        index, preset = state[name]
        return f"{name.upper()} = [{index}] {preset or 'Untitled'}"
    return f"{slot('a')}, {slot('b')}, morph = {state['morph']:.2f}"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--host", default="127.0.0.1", help="Ambidex's IP address (default: this machine)")
    parser.add_argument("--port", type=int, default=9000, help="Ambidex's OSC port (default: 9000)")
    parser.add_argument("--preset", type=int, help="index of the new preset A (default: a random one)")
    parser.add_argument("--seconds", type=float, default=5.0, help="duration of each morphing (default: 5)")
    args = parser.parse_args()

    ambidex = Ambidex(args.host, args.port)

    presets = ambidex.presets()
    print(f"{len(presets)} presets:")
    for index, name in enumerate(presets):
        print(f"  [{index}] {name}")

    state = ambidex.state()
    print(f"\nNow: {describe(state)}")
    print("Hold a few notes to hear the morphing...")
    time.sleep(2)

    print(f"\n1. Morphing to B ({args.seconds:g} s)")
    ambidex.morph_to(1.0, args.seconds)

    # Preset A isn't heard anymore: it can be changed without a jump in the sound
    new_preset = args.preset
    if new_preset is None:
        others = [i for i in range(len(presets)) if i not in (state["a"][0], state["b"][0])]
        new_preset = random.choice(others or range(len(presets)))
    print(f"2. Loading preset [{new_preset}] {presets[new_preset] if 0 <= new_preset < len(presets) else '?'} into A")
    ambidex.set_preset("a", new_preset)
    print(f"   Now: {describe(ambidex.state())}")

    print(f"3. Morphing back to A ({args.seconds:g} s)")
    ambidex.morph_to(0.0, args.seconds)

    print(f"\nDone: {describe(ambidex.state())}")


if __name__ == "__main__":
    try:
        main()
    except (AmbidexError, TimeoutError) as error:
        raise SystemExit(f"Error: {error}")
