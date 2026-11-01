// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex benchmark: the original scalar FM kernels, as references.

#pragma once

#include <cmath>
#include <cstdint>

#include "msfa/synth.h"
#include "msfa/sin.h"

// Copies of the scalar loops of Dexed's FmOpKernel (source/msfa/fm_op_kernel.cc)
// and EngineMkI (source/engine/EngineMkI.cpp), before the NEON kernels were added.
namespace ref {

inline void fm_compute(int32_t *output, const int32_t *input,
                       int32_t phase0, int32_t freq,
                       int32_t gain1, int32_t gain2, bool add) {
  int32_t dgain = (gain2 - gain1 + (N >> 1)) >> LG_N;
  int32_t gain = gain1;
  int32_t phase = phase0;
  if (add) {
    for (int i = 0; i < N; i++) {
      gain += dgain;
      int32_t y = Sin::lookup(phase + input[i]);
      int32_t y1 = ((int64_t)y * (int64_t)gain) >> 24;
      output[i] += y1;
      phase += freq;
    }
  } else {
    for (int i = 0; i < N; i++) {
      gain += dgain;
      int32_t y = Sin::lookup(phase + input[i]);
      int32_t y1 = ((int64_t)y * (int64_t)gain) >> 24;
      output[i] = y1;
      phase += freq;
    }
  }
}

inline void fm_compute_pure(int32_t *output, int32_t phase0, int32_t freq,
                            int32_t gain1, int32_t gain2, bool add) {
  int32_t dgain = (gain2 - gain1 + (N >> 1)) >> LG_N;
  int32_t gain = gain1;
  int32_t phase = phase0;
  if (add) {
    for (int i = 0; i < N; i++) {
      gain += dgain;
      int32_t y = Sin::lookup(phase);
      int32_t y1 = ((int64_t)y * (int64_t)gain) >> 24;
      output[i] += y1;
      phase += freq;
    }
  } else {
    for (int i = 0; i < N; i++) {
      gain += dgain;
      int32_t y = Sin::lookup(phase);
      int32_t y1 = ((int64_t)y * (int64_t)gain) >> 24;
      output[i] = y1;
      phase += freq;
    }
  }
}

//==============================================================================
static const uint16_t NEGATIVE_BIT = 0x8000;
static const uint16_t SINLOG_BITDEPTH = 10;
static const uint16_t SINLOG_TABLESIZE = 1<<SINLOG_BITDEPTH;
static const uint16_t SINEXP_BITDEPTH = 10;
static const uint16_t SINEXP_TABLESIZE = 1<<SINEXP_BITDEPTH;

struct MkITables {
    uint16_t sinLogTable[SINLOG_TABLESIZE];
    uint16_t sinExpTable[SINEXP_TABLESIZE];

    // (from the EngineMkI constructor)
    MkITables() {
        float bitReso = SINLOG_TABLESIZE;
        for(int i=0;i<SINLOG_TABLESIZE;i++) {
            float x1 = sin(((0.5+i)/bitReso) * M_PI/2.0);
            sinLogTable[i] = round(-1024 * log2(x1));
        }
        bitReso = SINEXP_TABLESIZE;
        for(int i=0;i<SINEXP_TABLESIZE;i++) {
            float x1 = (pow(2, float(i)/bitReso)-1) * 4096;
            sinExpTable[i] = round(x1);
        }
    }
};

inline const MkITables mki;

inline uint16_t sinLog(uint16_t phi) {
    const uint16_t SINLOG_TABLEFILTER = SINLOG_TABLESIZE-1;
    const uint16_t index = (phi & SINLOG_TABLEFILTER);

    switch( ( phi & (SINLOG_TABLESIZE * 3) ) ) {
        case 0:
            return mki.sinLogTable[index];
        case SINLOG_TABLESIZE:
            return mki.sinLogTable[index ^ SINLOG_TABLEFILTER];
        case SINLOG_TABLESIZE * 2 :
            return mki.sinLogTable[index] | NEGATIVE_BIT;
        default:
            return mki.sinLogTable[index ^ SINLOG_TABLEFILTER] | NEGATIVE_BIT;
    }
}

inline int32_t mkiSin(int32_t phase, uint16_t env) {
    uint16_t expVal = sinLog(phase >> (22 - SINLOG_BITDEPTH)) + (env);

    const bool isSigned = expVal & NEGATIVE_BIT;
    expVal &= ~NEGATIVE_BIT;

    const uint16_t SINEXP_FILTER = 0x3FF;
    uint16_t result = 4096 + mki.sinExpTable[( expVal & SINEXP_FILTER ) ^ SINEXP_FILTER];

    result >>= ( expVal >> 10 ); // exp

    if( isSigned )
        return (-result - 1) << 13;
    else
        return result << 13;
}

inline const int32_t zeros[N] = {0};

inline void mki_compute(int32_t *output, const int32_t *input,
                        int32_t phase0, int32_t freq,
                        int32_t gain1, int32_t gain2, bool add) {
    int32_t dgain = (gain2 - gain1 + (N >> 1)) >> LG_N;
    int32_t gain = gain1;
    int32_t phase = phase0;
    const int32_t *adder = add ? output : zeros;

    for (int i = 0; i < N; i++) {
        gain += dgain;
        int32_t y = mkiSin((phase+input[i]), gain);
        output[i] = y + adder[i];
        phase += freq;
    }
}

inline void mki_compute_pure(int32_t *output, int32_t phase0, int32_t freq,
                             int32_t gain1, int32_t gain2, bool add) {
    int32_t dgain = (gain2 - gain1 + (N >> 1)) >> LG_N;
    int32_t gain = gain1;
    int32_t phase = phase0;
    const int32_t *adder = add ? output : zeros;

    for (int i = 0; i < N; i++) {
        gain += dgain;
        int32_t y = mkiSin(phase , gain);
        output[i] = y + adder[i];
        phase += freq;
    }
}

} // namespace ref
