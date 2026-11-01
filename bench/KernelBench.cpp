// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex benchmark: FM kernels, checked against the original scalar code and timed.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#include "msfa/exp2.h"
#include "msfa/fm_core.h"
#include "msfa/fm_op_kernel.h"
#include "engine/EngineMkI.h"

#include "ReferenceKernels.h"

namespace {

// All kernels with the same signature (the input is ignored by the "pure" ones)
using KernelFn = void (*)(int32_t *output, const int32_t *input, int32_t phase0, int32_t freq,
                          int32_t gain1, int32_t gain2, bool add);

EngineMkI &mkiEngine() {
    static EngineMkI engine; // (its constructor fills the tables)
    return engine;
}

#define NOINLINE __attribute__((noinline))

NOINLINE void refFm(int32_t *o, const int32_t *in, int32_t p, int32_t f, int32_t g1, int32_t g2, bool a) {
    ref::fm_compute(o, in, p, f, g1, g2, a);
}
NOINLINE void refFmPure(int32_t *o, const int32_t *, int32_t p, int32_t f, int32_t g1, int32_t g2, bool a) {
    ref::fm_compute_pure(o, p, f, g1, g2, a);
}
NOINLINE void refMki(int32_t *o, const int32_t *in, int32_t p, int32_t f, int32_t g1, int32_t g2, bool a) {
    ref::mki_compute(o, in, p, f, g1, g2, a);
}
NOINLINE void refMkiPure(int32_t *o, const int32_t *, int32_t p, int32_t f, int32_t g1, int32_t g2, bool a) {
    ref::mki_compute_pure(o, p, f, g1, g2, a);
}

NOINLINE void curFm(int32_t *o, const int32_t *in, int32_t p, int32_t f, int32_t g1, int32_t g2, bool a) {
    FmOpKernel::compute(o, in, p, f, g1, g2, a);
}
NOINLINE void curFmPure(int32_t *o, const int32_t *, int32_t p, int32_t f, int32_t g1, int32_t g2, bool a) {
    FmOpKernel::compute_pure(o, p, f, g1, g2, a);
}
NOINLINE void curMki(int32_t *o, const int32_t *in, int32_t p, int32_t f, int32_t g1, int32_t g2, bool a) {
    mkiEngine().compute(o, in, p, f, g1, g2, a);
}
NOINLINE void curMkiPure(int32_t *o, const int32_t *, int32_t p, int32_t f, int32_t g1, int32_t g2, bool a) {
    mkiEngine().compute_pure(o, p, f, g1, g2, a);
}

// (unchanged, timed for comparison: the feedback operator stays scalar)
int32_t fbBuf[2];
NOINLINE void curFmFb(int32_t *o, const int32_t *, int32_t p, int32_t f, int32_t g1, int32_t g2, bool a) {
    FmOpKernel::compute_fb(o, p, f, g1, g2, fbBuf, 4, a);
}
NOINLINE void curMkiFb(int32_t *o, const int32_t *, int32_t p, int32_t f, int32_t g1, int32_t g2, bool a) {
    mkiEngine().compute_fb(o, p, f, g1, g2, fbBuf, 4, a);
}

struct Kernel {
    const char *name;
    KernelFn reference, current;
    bool mki; // gains are 14-bit attenuations instead of linear 2^24 gains
};

const Kernel kernels[] = {
    { "Modern compute", refFm, curFm, false },
    { "Modern compute_pure", refFmPure, curFmPure, false },
    { "Mark I compute", refMki, curMki, true },
    { "Mark I compute_pure", refMkiPure, curMkiPure, true },
};

struct Params {
    int32_t phase, freq, gain1, gain2;
    bool add;
};

// Typical values: 20 Hz - 5 kHz at 48 kHz, operator levels in their useful range
Params typicalParams(std::mt19937 &rng, bool mki) {
    std::uniform_int_distribution<int32_t> any;
    std::uniform_real_distribution<double> logFreq(std::log(20.0), std::log(5000.0));
    const auto freq = (int32_t)(std::exp(logFreq(rng)) / 48000.0 * (1 << 24));
    std::uniform_int_distribution<int32_t> gain = mki ? std::uniform_int_distribution<int32_t>(0, 16000)
                                                      : std::uniform_int_distribution<int32_t>(1120, 1 << 24);
    return { any(rng), freq, gain(rng), gain(rng), (rng() & 1) != 0 };
}

bool sameOutput(const Kernel &k, const int32_t *input, const int32_t *initial, const Params &p) {
    alignas(16) int32_t a[N], b[N];
    std::memcpy(a, initial, sizeof(a));
    std::memcpy(b, initial, sizeof(b));
    k.reference(a, input, p.phase, p.freq, p.gain1, p.gain2, p.add);
    k.current(b, input, p.phase, p.freq, p.gain1, p.gain2, p.add);
    return std::memcmp(a, b, sizeof(a)) == 0;
}

// Returns the number of calls with a different output
long checkKernel(const Kernel &k) {
    std::mt19937 rng(1234);
    std::uniform_int_distribution<int32_t> any;
    alignas(16) int32_t input[N], initial[N];
    long mismatches = 0;

    // 1. every phase (the lookups only use its 24 low bits), at a few gains
    for (int32_t gain : { 0, 1, 1000, 16383, 1 << 20, 1 << 24 }) {
        if (k.mki && gain > 16383)
            continue;
        std::fill(input, input + N, 0);
        std::fill(initial, initial + N, 0);
        for (int32_t phase = 0; phase < (1 << 24); phase += N)
            mismatches += !sameOutput(k, input, initial, { phase, 1, gain, gain, false });
    }

    // 2. random calls, typical then any values (wrapping phases, out of range gains)
    for (int i = 0; i < 1000000; ++i) {
        for (int j = 0; j < N; ++j) {
            input[j] = (i & 1) ? any(rng) : (any(rng) >> 6);
            initial[j] = any(rng) >> 4;
        }
        Params p = typicalParams(rng, k.mki);
        if (i & 1) {
            std::uniform_int_distribution<int32_t> gain(0, k.mki ? (1 << 20) : (1 << 27));
            p = { any(rng), any(rng), gain(rng), gain(rng), (rng() & 1) != 0 };
        }
        mismatches += !sameOutput(k, input, initial, p);
    }
    return mismatches;
}

// Best time over a few runs, in nanoseconds per call (one call = N samples)
double timeKernel(KernelFn fn, const std::vector<Params> &params, const int32_t *inputs) {
    alignas(16) int32_t output[N] = {};
    const int callsPerRun = 200000;
    double best = 1e30;
    int32_t sink = 0;

    for (int run = 0; run < 7; ++run) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < callsPerRun; ++i) {
            const Params &p = params[i % params.size()];
            fn(output, inputs + (i % 16) * N, p.phase, p.freq, p.gain1, p.gain2, p.add);
            sink += output[i % N];
        }
        const std::chrono::duration<double, std::nano> elapsed = std::chrono::steady_clock::now() - start;
        best = std::min(best, elapsed.count() / callsPerRun);
    }
    static volatile int32_t keep;
    keep = sink;
    return best;
}

// One voice (6 operators, all audible) through an engine's render(), averaged
// over the 32 algorithms, in nanoseconds per call (N samples)
double timeRender(FmCore &engine, bool mki) {
    std::mt19937 rng(42);
    FmOpParams params[32][6];
    for (auto &voice : params) {
        for (auto &op : voice) {
            const Params p = typicalParams(rng, mki);
            // level_in sets the gain: 2^23 to 2^24 for Modern, attenuations around 1000-2000 for Mark I
            op.level_in = mki ? (int32_t)(14 << 24) + (int32_t)(rng() % (1 << 24))
                              : (int32_t)(14 << 24) - (int32_t)(rng() % (1 << 24));
            op.gain_out = 0;
            op.freq = p.freq;
            op.phase = p.phase;
        }
    }
    alignas(16) int32_t output[N];
    int32_t fb[2] = {};
    const int blocks = 2000;
    double best = 1e30;

    for (int run = 0; run < 5; ++run) {
        const auto start = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b)
            for (int algorithm = 0; algorithm < 32; ++algorithm) {
                std::fill(output, output + N, 0);
                engine.render(output, params[algorithm], algorithm, fb, 4);
            }
        const std::chrono::duration<double, std::nano> elapsed = std::chrono::steady_clock::now() - start;
        best = std::min(best, elapsed.count() / (blocks * 32));
    }
    return best;
}

} // namespace

// Returns non-zero if a kernel isn't bit-exact
int runKernelBench(bool checkOnly) {
    Exp2::init();
    Sin::init();

#if defined(__ARM_NEON) && !defined(DEXED_NO_NEON)
    std::printf("Kernels: NEON\n");
#else
    std::printf("Kernels: scalar\n");
#endif

    // Bit-exactness
    long totalMismatches = 0;
    for (const auto &k : kernels) {
        const long mismatches = checkKernel(k);
        std::printf("check %-22s %s (%ld mismatching calls)\n", k.name, mismatches == 0 ? "OK" : "FAILED",
                    mismatches);
        totalMismatches += mismatches;
    }
    if (checkOnly || totalMismatches != 0)
        return totalMismatches != 0;

    // Timing
    std::mt19937 rng(99);
    std::uniform_int_distribution<int32_t> modulation(-(1 << 24), 1 << 24);
    std::vector<int32_t> inputs(16 * N);
    for (auto &x : inputs)
        x = modulation(rng);

    std::printf("\n%-22s %12s %12s %8s\n", "kernel (64 samples)", "scalar ns", "current ns", "speedup");
    for (const auto &k : kernels) {
        std::vector<Params> params;
        for (int i = 0; i < 256; ++i)
            params.push_back(typicalParams(rng, k.mki));
        const double tRef = timeKernel(k.reference, params, inputs.data());
        const double tCur = timeKernel(k.current, params, inputs.data());
        std::printf("%-22s %12.1f %12.1f %7.2fx\n", k.name, tRef, tCur, tRef / tCur);
        std::printf("csv,kernel,%s,%.2f,%.2f\n", k.name, tRef, tCur);
    }
    const Kernel feedbackKernels[] = { { "Modern compute_fb", nullptr, curFmFb, false },
                                       { "Mark I compute_fb", nullptr, curMkiFb, true } };
    for (const auto &[name, unused, fn, mki] : feedbackKernels) {
        std::vector<Params> params;
        for (int i = 0; i < 256; ++i)
            params.push_back(typicalParams(rng, mki));
        const double t = timeKernel(fn, params, inputs.data());
        std::printf("%-22s %12s %12.1f %8s\n", name, "-", t, "(scalar)");
        std::printf("csv,kernel,%s,,%.2f\n", name, t);
    }

    FmCore modern;
    const double tModern = timeRender(modern, false);
    const double tMkI = timeRender(mkiEngine(), true);
    std::printf("\nOne voice, 6 operators (render, 64 samples, mean of the 32 algorithms):\n");
    std::printf("  Modern %8.1f ns   Mark I %8.1f ns\n", tModern, tMkI);
    std::printf("csv,render,Modern,%.2f\ncsv,render,Mark I,%.2f\n", tModern, tMkI);
    return 0;
}
