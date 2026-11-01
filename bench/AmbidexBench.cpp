// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex benchmark: renders the app's audio processor headless and measures
// the time spent in processBlock. See bench/README.md.

#include <JuceHeader.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <fcntl.h>
#include <limits>
#include <unistd.h>
#include <vector>

#include "AmbidexProcessor.h"

int runKernelBench(bool checkOnly);

// Dexed leaves a few members uninitialised (Lfo::phase_ and randstate_,
// Dx7Note::fb_buf_), and its objects are allocated on the heap: zeroed
// allocations make the rendered audio the same from run to run, and so
// comparable between builds.
void *operator new(std::size_t size) {
    if (void *p = std::calloc(1, size ? size : 1))
        return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size) { return operator new(size); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

namespace {

constexpr double sampleRate = 48000.0;

enum class Playing {
    held,     // `notes` keys held down during the whole run: exactly that many voices
    chords,   // chords of `notes` notes struck every second, moving up: all 16 voices cycle
    released, // a chord of `notes` notes released at 0.9 s, measured from 1 s: the release tails
};

struct Scenario {
    String name;
    int engineType; // DEXED_ENGINE_MODERN or DEXED_ENGINE_MARKI
    Playing playing;
    int notes;      // played by both engines (16 voices max per engine)
};

struct Result {
    double cpuPercent;   // processBlock time / audio duration, for one core
    double p99Percent;   // 99th percentile and worst block, relative to the block duration
    double maxPercent;
    uint64_t hash;       // of the rendered audio, to compare builds
};

// FNV-1a over the bits of the samples
void hashSamples(uint64_t &hash, const AudioBuffer<float> &buffer) {
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        const auto *bytes = reinterpret_cast<const uint8_t *>(buffer.getReadPointer(ch));
        for (size_t i = 0; i < buffer.getNumSamples() * sizeof(float); ++i)
            hash = (hash ^ bytes[i]) * 0x100000001b3ULL;
    }
}

int chordPitch(int note, int notes, int chord) {
    // spread over 4 octaves, moving up a little at each chord (distinct pitches)
    return 36 + (note * 47 / std::max(1, notes - 1) + chord * 2) % 48;
}

// Chords struck every second and released after 0.9 s (until `end`)
void addChordEvents(MidiBuffer &midi, int notes, int64_t blockStart, int blockSize,
                    int64_t end = std::numeric_limits<int64_t>::max()) {
    const int64_t period = (int64_t)sampleRate;
    const int64_t releaseAt = period * 9 / 10;
    for (int64_t pos = blockStart; pos < std::min(blockStart + blockSize, end); ++pos) {
        const int64_t phase = pos % period;
        if (phase != 0 && phase != releaseAt)
            continue;
        for (int n = 0; n < notes; ++n) {
            const int pitch = chordPitch(n, notes, (int)(pos / period));
            midi.addEvent(phase == 0 ? MidiMessage::noteOn(1, pitch, (uint8)100) : MidiMessage::noteOff(1, pitch),
                          (int)(pos - blockStart));
        }
    }
}

// Runs `function` with stdout redirected to /dev/null (Dexed logs to stdout)
template <typename Function>
void withoutStdout(Function function) {
    std::fflush(stdout);
    const int saved = dup(STDOUT_FILENO);
    const int devNull = open("/dev/null", O_WRONLY);
    dup2(devNull, STDOUT_FILENO);
    function();
    std::fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(devNull);
    close(saved);
}

std::unique_ptr<AmbidexAudioProcessor> createAmbidex(int blockSize) {
    std::unique_ptr<AmbidexAudioProcessor> ambidex;
    withoutStdout([&] { ambidex = std::make_unique<AmbidexAudioProcessor>(); });
    ambidex->setPlayConfigDetails(0, 2, sampleRate, blockSize);
    return ambidex;
}

Result runScenario(const Scenario &s, int blockSize, double seconds) {
    // (a new instance for each run: the same audio every time)
    auto ambidex = createAmbidex(blockSize);
    for (int i = 0; i < AmbidexAudioProcessor::numEngines; ++i)
        ambidex->getEngine(i).setEngineType(s.engineType);
    ambidex->prepareToPlay(sampleRate, blockSize);

    AudioBuffer<float> buffer(2, blockSize);
    MidiBuffer midi;
    int64_t position = 0;

    if (s.playing == Playing::released) {
        // one chord (released after 0.9 s), measured from 1 s
        for (; position < (int64_t)sampleRate; position += blockSize) {
            midi.clear();
            addChordEvents(midi, s.notes, position, blockSize, (int64_t)sampleRate);
            ambidex->processBlock(buffer, midi);
        }
    }

    const int numBlocks = (int)(seconds * sampleRate / blockSize);
    std::vector<double> blockTimes;
    blockTimes.reserve(numBlocks);
    uint64_t hash = 0xcbf29ce484222325ULL;

    for (int b = 0; b < numBlocks; ++b, position += blockSize) {
        midi.clear();
        if (s.playing == Playing::chords)
            addChordEvents(midi, s.notes, position, blockSize);
        else if (s.playing == Playing::held && b == 0)
            for (int n = 0; n < s.notes; ++n)
                midi.addEvent(MidiMessage::noteOn(1, chordPitch(n, s.notes, 0), (uint8)100), 0);
        buffer.clear();

        const auto start = std::chrono::steady_clock::now();
        ambidex->processBlock(buffer, midi);
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;

        blockTimes.push_back(elapsed.count());
        hashSamples(hash, buffer);
    }

    const double blockDuration = blockSize / sampleRate;
    double total = 0;
    for (double t : blockTimes)
        total += t;
    std::sort(blockTimes.begin(), blockTimes.end());
    return { 100.0 * total / (numBlocks * blockDuration),
             100.0 * blockTimes[(size_t)(0.99 * (numBlocks - 1))] / blockDuration,
             100.0 * blockTimes.back() / blockDuration,
             hash };
}

int runAppBench(int blockSize, double seconds, int repeats) {
    // Ambidex uses the "Ambidex" folder next to the executable if it exists:
    // the benchmark then starts from the default settings, on every machine
    File::getSpecialLocation(File::currentExecutableFile).getSiblingFile("Ambidex").createDirectory();

    {
        auto ambidex = createAmbidex(blockSize);
        for (int i = 0; i < AmbidexAudioProcessor::numEngines; ++i) {
            auto &engine = ambidex->getEngine(i);
            std::printf("Engine %c: \"%s\", engine type at startup: %d (0 = Modern, 1 = Mark I)\n", 'A' + i,
                        engine.getProgramName(engine.getCurrentProgram()).trim().toRawUTF8(),
                        engine.getEngineType());
        }
    }
    std::printf("Block: %d samples at %.0f Hz (%.2f ms), %.0f s per run, best of %d runs\n\n", blockSize, sampleRate,
                1000.0 * blockSize / sampleRate, seconds, repeats);

    std::vector<Scenario> scenarios;
    for (int engineType : { DEXED_ENGINE_MODERN, DEXED_ENGINE_MARKI }) {
        const String engineName = engineType == DEXED_ENGINE_MODERN ? "Modern" : "MarkI";
        for (int notes : { 1, 4, 8, 16 })
            scenarios.push_back({ engineName + " held " + String(notes), engineType, Playing::held, notes });
        scenarios.push_back({ engineName + " chords 8", engineType, Playing::chords, 8 });
        scenarios.push_back({ engineName + " released 16", engineType, Playing::released, 16 });
    }

    std::printf("%-22s %8s %8s %8s  %s\n", "scenario (x2 engines)", "CPU %", "p99 %", "max %", "audio hash");
    for (const auto &s : scenarios) {
        Result best {};
        for (int r = 0; r < repeats; ++r) {
            const Result result = runScenario(s, blockSize, seconds);
            if (r == 0 || result.cpuPercent < best.cpuPercent)
                best = result;
            if (result.hash != best.hash)
                std::printf("warning: the audio of %s differs between runs\n", s.name.toRawUTF8());
        }
        std::printf("%-22s %8.2f %8.2f %8.2f  %016llx\n", s.name.toRawUTF8(), best.cpuPercent, best.p99Percent,
                    best.maxPercent, (unsigned long long)best.hash);
        std::printf("csv,app,%s,%.3f,%.3f,%.3f,%016llx\n", s.name.toRawUTF8(), best.cpuPercent, best.p99Percent,
                    best.maxPercent, (unsigned long long)best.hash);
    }
    return 0;
}

void printUsage() {
    std::printf("usage: ambidex-bench [kernel|check|app] [--block N] [--seconds S] [--repeats R]\n"
                "  kernel  checks the FM kernels against the original scalar code, then times them\n"
                "  check   only checks the kernels (exit code 1 if not bit-exact)\n"
                "  app     renders Ambidex (2 engines) for several scenarios and measures the CPU load\n"
                "  (default: kernel, then app)\n");
}

} // namespace

int main(int argc, char *argv[]) {
    String mode;
    int blockSize = 256;
    double seconds = 10.0;
    int repeats = 3;

    for (int i = 1; i < argc; ++i) {
        const String arg(argv[i]);
        if (arg == "--block" && i + 1 < argc)
            blockSize = String(argv[++i]).getIntValue();
        else if (arg == "--seconds" && i + 1 < argc)
            seconds = String(argv[++i]).getDoubleValue();
        else if (arg == "--repeats" && i + 1 < argc)
            repeats = String(argv[++i]).getIntValue();
        else if (arg == "kernel" || arg == "check" || arg == "app")
            mode = arg;
        else {
            printUsage();
            return 2;
        }
    }
    if (blockSize <= 0 || seconds <= 0 || repeats <= 0) {
        printUsage();
        return 2;
    }

    if (mode != "app") {
        if (const int failed = runKernelBench(mode == "check"))
            return failed;
        std::printf("\n");
    }
    if (mode == "" || mode == "app") {
        ScopedJuceInitialiser_GUI juce;
        return runAppBench(blockSize, seconds, repeats);
    }
    return 0;
}
