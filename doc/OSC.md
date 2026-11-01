OSC remote control
==================

Ambidex listens for [OSC](https://opensoundcontrol.stanford.edu/) messages on **UDP port 9000**,
on all network interfaces, to set the morph and the presets of slots A and B from another program,
on this machine or on the network. If the port is already used by another app (or another
Ambidex), the OSC control is off (macOS and Linux; on Windows, both apps may share the port).
`scripts/osc_demo.py` is a Python client and demo.

Messages
--------

| Address                  | Arguments          | Effect                                              |
|--------------------------|--------------------|-----------------------------------------------------|
| `/ambidex/morph`         | number (0 to 1)    | Sets the morph: 0 = only A is heard, 1 = only B     |
| `/ambidex/slot/a/preset` | preset index       | Loads a library preset into slot A                  |
| `/ambidex/slot/b/preset` | preset index       | Loads a library preset into slot B                  |
| `/ambidex/presets/list`  | -                  | Replies `/ambidex/presets`, with the preset names   |
| `/ambidex/state`         | -                  | Replies `/ambidex/state`, see below                 |

Numbers can be sent as int or float (`i`, `f`, `h` or `d`); a preset index must be a whole number.
Messages can also be grouped in bundles (their time tags are ignored: they're handled right away).
Messages are handled in the order they're received: e.g. a preset load followed by a morph.

* **Preset indices** are positions in the list returned by `/ambidex/presets/list`, from 0. The
  list is sorted by name, so adding, renaming or deleting a preset shifts the indices after it:
  fetch the list again after such changes.
* **Loading a preset** discards the unsaved edits of the slot, without asking. To change a
  preset without a jump in the sound, load it into the slot that isn't heard (morph at the other
  end).
* **The morph** is smoothed over 50 ms by Ambidex, so a controller can send steps at a few tens of
  messages per second and still get a smooth crossfade. Values outside 0 to 1 are clamped.

Replies
-------

Replies go back to the address and port the query came from: a client only needs one UDP socket,
to send its messages and receive the replies.

* `/ambidex/presets` `s s s ...`: the names of the library presets, in index order.
* `/ambidex/state` `i s i s f`: index and name of preset A, index and name of preset B, morph.
  The index is -1 if the slot's preset isn't in the library (deleted, or never saved).
* `/ambidex/error` `s`: a message couldn't be handled (unknown address, wrong arguments, preset
  index out of range...). Commands don't reply when they succeed: an error comes before the reply
  to the next query.

Clients
-------

Python, with [python-osc](https://pypi.org/project/python-osc/) (`pip install python-osc`):

```python
from pythonosc.udp_client import SimpleUDPClient

ambidex = SimpleUDPClient("127.0.0.1", 9000)
ambidex.send_message("/ambidex/slot/a/preset", 3)
ambidex.send_message("/ambidex/morph", 0.5)
```

For the queries, see the `Ambidex` class in `scripts/osc_demo.py` (one socket for the commands
and their replies). The demo morphs from A to B, loads another preset into A, then morphs back
(in the venv described in the README):

```
.venv/bin/python scripts/osc_demo.py [--host 192.168.1.20] [--port 9000] [--preset 12] [--seconds 5]
```

C, with [liblo](https://liblo.sourceforge.net/) (`brew install liblo`, `apt install liblo-dev`):

```c
lo_address ambidex = lo_address_new("127.0.0.1", "9000");
lo_send(ambidex, "/ambidex/slot/a/preset", "i", 3);
lo_send(ambidex, "/ambidex/morph", "f", 0.5f);

// A query, sent from a local server that receives the reply
lo_server server = lo_server_new(NULL, NULL);
lo_server_add_method(server, "/ambidex/state", "isisf", on_state, NULL);
lo_send_from(ambidex, server, LO_TT_IMMEDIATE, "/ambidex/state", "");
lo_server_recv_noblock(server, 1000); // calls on_state()
```

Without a library, an OSC message is simple to build by hand: the address, the type tags (e.g.
`,f`), each padded with nulls to a multiple of 4 bytes, then the arguments in big-endian order.
