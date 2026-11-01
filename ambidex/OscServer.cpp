// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#include "OscServer.h"
#include "AmbidexProcessor.h"

#if ! JUCE_WINDOWS
 #include <sys/socket.h>
#endif

namespace {
const String morphAddress("/ambidex/morph");
const String slotPresetAddresses[] = { "/ambidex/slot/a/preset", "/ambidex/slot/b/preset" };
const String listPresetsAddress("/ambidex/presets/list");
const String stateAddress("/ambidex/state");

// Replies
const char* const presetsReplyAddress = "/ambidex/presets";
const char* const stateReplyAddress = "/ambidex/state";
const char* const errorReplyAddress = "/ambidex/error";

// The largest UDP payload
constexpr int maxPacketSize = 65536;
// Messages waiting for the message thread, beyond which new ones are dropped (flood)
constexpr int maxUnhandled = 1000;

/** Reads OSC data: big-endian, padded to multiples of 4 bytes. */
struct OscReader {
    const uint8* data;
    size_t size;
    size_t pos = 0;
    bool failed = false;

    bool atEnd() const { return pos >= size; }

    void skip(size_t numBytes) {
        numBytes = (numBytes + 3) & ~(size_t) 3;
        if (size - pos < numBytes) {
            failed = true;
            pos = size;
        } else {
            pos += numBytes;
        }
    }

    uint32 readUint32() {
        if (size - pos < 4) {
            failed = true;
            pos = size;
            return 0;
        }
        const uint32 value = ByteOrder::bigEndianInt(data + pos);
        pos += 4;
        return value;
    }

    uint64 readUint64() {
        const uint64 high = readUint32();
        return (high << 32) | readUint32();
    }

    String readString() {
        const auto* start = data + pos;
        const auto* end = std::find(start, data + size, (uint8) 0);
        if (end == data + size) {
            failed = true;
            pos = size;
            return {};
        }
        const auto length = (size_t) (end - start);
        skip(length + 1); // (with the terminating null)
        return String::fromUTF8((const char*) start, (int) length);
    }
};

/** Decodes a packet: a message, or a bundle of them (whose time tags are ignored). */
void decodePacket(const uint8* data, size_t size, std::vector<OscServer::Message>& messages, int depth = 0) {
    OscReader reader { data, size };

    if (size >= 16 && std::memcmp(data, "#bundle", 8) == 0) {
        if (depth > 8)
            return;
        reader.skip(16); // "#bundle" and the time tag
        while (!reader.atEnd()) {
            const size_t elementSize = reader.readUint32();
            if (reader.failed || elementSize > size - reader.pos)
                return;
            decodePacket(data + reader.pos, elementSize, messages, depth + 1);
            reader.skip(elementSize);
        }
        return;
    }

    OscServer::Message message;
    message.address = reader.readString();
    if (reader.failed || !message.address.startsWithChar('/'))
        return;

    // (very old clients don't send type tags: no arguments then)
    const String typeTags = reader.atEnd() ? String(",") : reader.readString();
    if (reader.failed || !typeTags.startsWithChar(','))
        return;

    for (int i = 1; i < typeTags.length(); ++i) {
        switch (typeTags[i]) {
            case 'i':
                message.args.add((int) (int32) reader.readUint32());
                break;
            case 'h':
                message.args.add((int64) reader.readUint64());
                break;
            case 'f': {
                const uint32 bits = reader.readUint32();
                float value;
                std::memcpy(&value, &bits, sizeof(value));
                message.args.add((double) value);
                break;
            }
            case 'd': {
                const uint64 bits = reader.readUint64();
                double value;
                std::memcpy(&value, &bits, sizeof(value));
                message.args.add(value);
                break;
            }
            case 's':
            case 'S':
                message.args.add(reader.readString());
                break;
            case 'T':
                message.args.add(true);
                break;
            case 'F':
                message.args.add(false);
                break;
            case 'N':
            case 'I':
                message.args.add(var());
                break;
            case 'b': // (not used by Ambidex)
                reader.skip(reader.readUint32());
                message.args.add(var());
                break;
            default: // unknown size: the rest can't be read
                return;
        }
    }

    if (!reader.failed)
        messages.push_back(std::move(message));
}

bool isNumber(const var& value) {
    return value.isInt() || value.isInt64() || value.isDouble();
}

String describeArgs(const Array<var>& args) {
    StringArray types;
    for (const auto& arg : args)
        types.add(arg.isString()  ? "string"
                  : isNumber(arg) ? (std::isfinite((double) arg) ? "number" : "non-finite number")
                  : arg.isBool()  ? "bool"
                                  : "nil");
    return types.isEmpty() ? String("no arguments") : types.joinIntoString(", ");
}
} // namespace

//==============================================================================
OscServer::OscServer(AmbidexAudioProcessor& p, int portToUse)
    : Thread("OSC server"), processor(p), port(portToUse) {
#if ! JUCE_WINDOWS
    // JUCE's sockets allow the port to be shared: another app (or Ambidex instance)
    // could then silently get part of the messages. Binding must fail instead.
    const int reuse = 0;
    setsockopt(socket.getRawSocketHandle(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

    // (on all network interfaces)
    listening = socket.bindToPort(port);
    if (!listening) {
        DBG("OSC: unable to listen on UDP port " << port);
        return;
    }

    // (replies are sent from the same socket, to each request's sender)
    sender.connectToSocket(socket, {}, 0);
    startThread();
}

OscServer::~OscServer() {
    signalThreadShouldExit();
    // (unblocks the network thread)
    socket.shutdown();
    stopThread(2000);
    cancelPendingUpdate();
}

void OscServer::run() {
    HeapBlock<uint8> buffer(maxPacketSize);

    while (!threadShouldExit()) {
        // (with a timeout, to check threadShouldExit)
        const int ready = socket.waitUntilReady(true, 100);
        if (ready < 0) {
            // (e.g. an error reported for a previous reply: the socket still works)
            Thread::sleep(10);
            continue;
        }
        if (ready == 0)
            continue;

        String senderAddress;
        int senderPort = 0;
        const int size = socket.read(buffer, maxPacketSize, false, senderAddress, senderPort);
        if (size <= 0)
            continue;

        std::vector<Message> messages;
        decodePacket(buffer, (size_t) size, messages);

        bool queued = false;
        for (auto& message : messages) {
            message.senderAddress = senderAddress;
            message.senderPort = senderPort;

            // The morph may be streamed by a controller, and its parameter is
            // thread safe: no detour through the message thread, unless messages
            // received before are still waiting there (e.g. a preset to load
            // before morphing to it)
            if (message.address == morphAddress && numUnhandled == 0) {
                setMorph(message);
            } else if (numUnhandled < maxUnhandled) {
                const ScopedLock lock(pendingLock);
                pending.push_back(std::move(message));
                ++numUnhandled;
                queued = true;
            }
        }
        if (queued)
            triggerAsyncUpdate();
    }
}

void OscServer::handleAsyncUpdate() {
    std::vector<Message> messages;
    {
        const ScopedLock lock(pendingLock);
        std::swap(messages, pending);
    }
    for (const auto& message : messages) {
        handleMessage(message);
        --numUnhandled;
    }
}

//==============================================================================
void OscServer::setMorph(const Message& message) {
    if (message.args.size() != 1 || !isNumber(message.args[0]) || !std::isfinite((double) message.args[0])) {
        replyError(message, morphAddress + " expects a number (0 = A, 1 = B), got " + describeArgs(message.args));
        return;
    }

    // (the editor follows, through the slider's attachment)
    auto& morph = *processor.morph;
    const float value = (float) jlimit(0.0, 1.0, (double) message.args[0]);
    morph.setValueNotifyingHost(morph.convertTo0to1(value));
}

void OscServer::handleMessage(const Message& message) {
    const auto& address = message.address;

    if (address == morphAddress) {
        setMorph(message);
    } else if (address == listPresetsAddress) {
        OSCMessage presets(presetsReplyAddress);
        for (const auto& name : processor.getPresetLibrary().getNames())
            presets.addString(name);
        reply(message, presets);
    } else if (address == stateAddress) {
        // Index and name of each slot's preset (index -1 if it isn't in the library), then the morph
        const StringArray names = processor.getPresetLibrary().getNames();
        OSCMessage state(stateReplyAddress);
        for (int slot = 0; slot < AmbidexAudioProcessor::numEngines; ++slot) {
            const auto& name = processor.getPresetName(slot);
            state.addInt32(name.isEmpty() ? -1 : names.indexOf(name));
            state.addString(name);
        }
        state.addFloat32(processor.morph->get());
        reply(message, state);
    } else if (address == slotPresetAddresses[0]) {
        loadPreset(message, 0);
    } else if (address == slotPresetAddresses[1]) {
        loadPreset(message, 1);
    } else {
        replyError(message, "unknown address " + address);
    }
}

void OscServer::loadPreset(const Message& message, int slot) {
    // (some controllers only send floats: a whole number is accepted)
    const double value = message.args.size() == 1 && isNumber(message.args[0]) ? (double) message.args[0] : 0.5;
    if (std::floor(value) != value) {
        replyError(message, message.address + " expects a preset index, got " + describeArgs(message.args));
        return;
    }

    auto& library = processor.getPresetLibrary();
    const StringArray names = library.getNames();
    // (checked before the conversion to int, which would overflow)
    if (!isPositiveAndBelow(value, (double) names.size())) {
        replyError(message, "preset index " + String(value, 0) + " out of range (0 to " + String(names.size() - 1)
                                + ", see " + listPresetsAddress + ")");
        return;
    }
    const int index = (int) value;

    // (discards the slot's unsaved edits, if any)
    AmbidexPreset preset;
    if (!library.load(names[index], preset)) {
        replyError(message, "unable to load the preset \"" + names[index] + "\"");
        return;
    }
    processor.loadPreset(slot, preset);

    if (onPresetLoaded != nullptr)
        onPresetLoaded(slot);
}

//==============================================================================
void OscServer::reply(const Message& request, const OSCMessage& message) {
    const ScopedLock lock(sendLock);
    if (!sender.sendToIPAddress(request.senderAddress, request.senderPort, message))
        DBG("OSC: unable to reply to " << request.senderAddress << ":" << request.senderPort);
}

void OscServer::replyError(const Message& request, const String& error) {
    reply(request, OSCMessage(errorReplyAddress, error));
}
