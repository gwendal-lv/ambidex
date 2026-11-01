// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#pragma once

#include <JuceHeader.h>

class AmbidexAudioProcessor;

/**
 * Remote control over OSC (UDP, on all network interfaces): the morph, and the
 * presets of slots A and B. See doc/OSC.md.
 *
 * Queries are answered to the address and port they came from, so that a client
 * only needs one socket. JUCE's OSCReceiver doesn't tell the sender: packets are
 * read from the socket here, and decoded by a minimal OSC parser.
 */
class OscServer : private Thread, private AsyncUpdater {
public:
    static constexpr int defaultPort = 9000;

    /** An OSC message. Arguments: int, int64, double (also for OSC floats), String or bool. */
    struct Message {
        String address;
        Array<var> args;
        // Where the packet came from (for the replies)
        String senderAddress;
        int senderPort = 0;
    };

    OscServer(AmbidexAudioProcessor& processor, int port = defaultPort);
    ~OscServer() override;

    /** False if the port couldn't be opened (e.g. used by another app). */
    bool isListening() const { return listening; }
    int getPort() const { return port; }

    // Called on the message thread when a preset was loaded into a slot (0 = A, 1 = B)
    std::function<void(int slot)> onPresetLoaded;

private:
    void run() override;
    void handleAsyncUpdate() override;

    // Network thread: the morph is set right away
    void setMorph(const Message& message);
    // Message thread: everything else
    void handleMessage(const Message& message);
    void loadPreset(const Message& message, int slot);

    // Any thread
    void reply(const Message& request, const OSCMessage& message);
    void replyError(const Message& request, const String& error);

    AmbidexAudioProcessor& processor;
    const int port;
    DatagramSocket socket;
    bool listening = false;

    CriticalSection sendLock;
    OSCSender sender;

    // Received by the network thread, for the message thread
    CriticalSection pendingLock;
    std::vector<Message> pending;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscServer)
};
