/*
 * Copyright (C) 2015-2017 Pascal Gauthier.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 *
 * The code is based on ppplay https://github.com/stohrendorf/ppplay and opl3
 * math documentation :
 * https://github.com/gtaylormb/opl3_fpga/blob/master/docs/opl3math/opl3math.pdf
 *
 */

#include "EngineMkI.h"
#include <cmath>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "msfa/sin.h"
#include "msfa/exp2.h"

#ifdef DEBUG
    #include "time.h"
    //#define MKIDEBUG
#endif

#ifdef _WIN32
#if _MSC_VER < 1800
    double log2(double n)  {  
        return log(n) / log(2.0);  
    }
    double round(double n) {
        return n < 0.0 ? ceil(n - 0.5) : floor(n + 0.5);
    }
#endif
    __declspec(align(16)) const int zeros[N] = {0};
#else
    const int32_t __attribute__ ((aligned(16))) zeros[N] = {0};
#endif

static const uint16_t NEGATIVE_BIT = 0x8000;
static const uint16_t ENV_BITDEPTH = 14;

static const uint16_t SINLOG_BITDEPTH = 10;
static const uint16_t SINLOG_TABLESIZE = 1<<SINLOG_BITDEPTH;
static uint16_t sinLogTable[SINLOG_TABLESIZE];

static const uint16_t SINEXP_BITDEPTH = 10;
static const uint16_t SINEXP_TABLESIZE = 1<<SINEXP_BITDEPTH;
static uint16_t sinExpTable[SINEXP_TABLESIZE];

const uint16_t ENV_MAX = 1<<ENV_BITDEPTH;

static inline uint16_t sinLog(uint16_t phi) {
    const uint16_t SINLOG_TABLEFILTER = SINLOG_TABLESIZE-1;
    const uint16_t index = (phi & SINLOG_TABLEFILTER);
    
    switch( ( phi & (SINLOG_TABLESIZE * 3) ) ) {
        case 0:
            return sinLogTable[index];
        case SINLOG_TABLESIZE:
            return sinLogTable[index ^ SINLOG_TABLEFILTER];
        case SINLOG_TABLESIZE * 2 :
            return sinLogTable[index] | NEGATIVE_BIT;
        default:
            return sinLogTable[index ^ SINLOG_TABLEFILTER] | NEGATIVE_BIT;
    }
}

EngineMkI::EngineMkI() {
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
    
#ifdef MKIDEBUG
    char buffer[4096];
    int pos = 0;
    
    TRACE("****************************************");
    for(int i=0;i<SINLOG_TABLESIZE;i++) {
        pos += sprintf(buffer+pos, "%d ", sinLogTable[i]);
        if ( pos > 90 ) {
            TRACE("SINLOGTABLE: %s" ,buffer);
            buffer[0] = 0;
            pos = 0;
        }
    }
    TRACE("SINLOGTABLE: %s", buffer);
    buffer[0] = 0;
    pos = 0;
    TRACE("----------------------------------------");    
    for(int i=0;i<SINEXP_TABLESIZE;i++) {
        pos += sprintf(buffer+pos, "%d ", sinExpTable[i]);
        if ( pos > 90 ) {
            TRACE("SINEXTTABLE: %s" ,buffer);
            buffer[0] = 0;
            pos = 0;
        }
    }
    TRACE("SINEXTTABLE: %s", buffer);
    TRACE("****************************************");
#endif
}

inline int32_t mkiSin(int32_t phase, uint16_t env) {
    uint16_t expVal = sinLog(phase >> (22 - SINLOG_BITDEPTH)) + (env);
    //int16_t expValShow = expVal;
    
    const bool isSigned = expVal & NEGATIVE_BIT;
    expVal &= ~NEGATIVE_BIT;
    
    const uint16_t SINEXP_FILTER = 0x3FF;
    uint16_t result = 4096 + sinExpTable[( expVal & SINEXP_FILTER ) ^ SINEXP_FILTER];
    
    //uint16_t resultB4 = result;
    result >>= ( expVal >> 10 ); // exp
    
#ifdef MKIDEBUG
    if ( ( time(NULL) % 5 ) == 0 ) {
        if ( expValShow < 0 ) {
            expValShow = (expValShow + 0x7FFF) * -1;
        }
        //TRACE(",%d,%d,%d,%d,%d,%d", phase >> (22 - SINLOG_BITDEPTH), env, expValShow, ( expVal & SINEXP_FILTER ) ^ SINEXP_FILTER, resultB4, result);
    }
#endif
    
    if( isSigned )
        return (-result - 1) << 13;
    else
        return result << 13;
}

// NEON (always available on aarch64) renders 4 samples at a time. Define
// DEXED_NO_NEON to use the scalar loops instead.
#if defined(__ARM_NEON) && !defined(DEXED_NO_NEON)
#define DEXED_NEON_MKI_KERNEL
#include <arm_neon.h>

// table[index] for each lane (there is no gather instruction)
static inline uint32x4_t gather4(const uint16_t *table, uint32x4_t index) {
    uint32x4_t v = vdupq_n_u32(table[vgetq_lane_u32(index, 0)]);
    v = vsetq_lane_u32(table[vgetq_lane_u32(index, 1)], v, 1);
    v = vsetq_lane_u32(table[vgetq_lane_u32(index, 2)], v, 2);
    return vsetq_lane_u32(table[vgetq_lane_u32(index, 3)], v, 3);
}

// mkiSin(phase, env) for 4 samples (its uint16_t maths done in 32-bit lanes)
static inline int32x4_t neon_mkiSin(uint32x4_t phase, uint32x4_t env) {
    // sinLog(phase >> 12): quarter wave table, mirrored in the 2nd and
    // 4th quarters, negative in the 2nd half
    uint32x4_t phi = vshrq_n_u32(phase, 22 - SINLOG_BITDEPTH);
    uint32x4_t mirror = vtstq_u32(phi, vdupq_n_u32(SINLOG_TABLESIZE));
    uint32x4_t index = veorq_u32(vandq_u32(phi, vdupq_n_u32(SINLOG_TABLESIZE - 1)),
                                 vandq_u32(mirror, vdupq_n_u32(SINLOG_TABLESIZE - 1)));
    uint32x4_t negative = vandq_u32(vshlq_n_u32(phi, 4), vdupq_n_u32(NEGATIVE_BIT)); // (bit 11 -> 15)
    uint32x4_t expVal = vorrq_u32(gather4(sinLogTable, index), negative);

    // + env, as uint16_t
    expVal = vandq_u32(vaddq_u32(expVal, env), vdupq_n_u32(0xFFFF));
    uint32x4_t isSigned = vtstq_u32(expVal, vdupq_n_u32(NEGATIVE_BIT));
    expVal = vandq_u32(expVal, vdupq_n_u32(NEGATIVE_BIT - 1));

    const uint32x4_t SINEXP_FILTER = vdupq_n_u32(0x3FF);
    uint32x4_t result = vaddq_u32(vdupq_n_u32(4096),
        gather4(sinExpTable, veorq_u32(vandq_u32(expVal, SINEXP_FILTER), SINEXP_FILTER)));
    // result >>= (expVal >> 10), a negative shift is a right shift
    result = vshlq_u32(result, vnegq_s32(vreinterpretq_s32_u32(vshrq_n_u32(expVal, 10))));

    // (-result - 1) << 13 == ~result << 13
    return vreinterpretq_s32_u32(vshlq_n_u32(veorq_u32(result, isSigned), 13));
}

// compute() and compute_pure() for N samples, bit-exact with the scalar loops.
// Sample i uses phase0 + i * freq (+ input[i]) and gain1 + (i + 1) * dgain.
template <bool hasInput, bool add>
static inline void neon_mki_kernel(int32_t *output, const int32_t *input,
                                   int32_t phase0, int32_t freq,
                                   int32_t gain1, int32_t dgain) {
    const uint32_t lanes[4] = { 0, 1, 2, 3 };
    const uint32x4_t lane = vld1q_u32(lanes);
    // (unsigned maths: the phase wraps around)
    uint32x4_t phase = vmlaq_n_u32(vdupq_n_u32((uint32_t)phase0), lane, (uint32_t)freq);
    uint32x4_t gain = vmlaq_n_u32(vdupq_n_u32((uint32_t)gain1 + (uint32_t)dgain), lane, (uint32_t)dgain);
    const uint32x4_t phaseStep = vdupq_n_u32(4 * (uint32_t)freq);
    const uint32x4_t gainStep = vdupq_n_u32(4 * (uint32_t)dgain);

    // 16 samples per iteration: the out-of-order core overlaps the table loads
    // of the 4 groups (much faster than 4 samples per iteration on a Cortex-A72)
    const int unroll = 4;
    for (int i = 0; i < N; i += 4 * unroll) {
        int32x4_t y[unroll];
        for (int u = 0; u < unroll; u++) {
            uint32x4_t p = hasInput ? vaddq_u32(phase, vreinterpretq_u32_s32(vld1q_s32(input + i + 4 * u))) : phase;
            y[u] = neon_mkiSin(p, gain);
            phase = vaddq_u32(phase, phaseStep);
            gain = vaddq_u32(gain, gainStep);
        }
        for (int u = 0; u < unroll; u++) {
            if (add)
                y[u] = vaddq_s32(y[u], vld1q_s32(output + i + 4 * u));
            vst1q_s32(output + i + 4 * u, y[u]);
        }
    }
}
#endif

void EngineMkI::compute(int32_t *output, const int32_t *input,
                        int32_t phase0, int32_t freq,
                        int32_t gain1, int32_t gain2, bool add) {
    int32_t dgain = (gain2 - gain1 + (N >> 1)) >> LG_N;
#ifdef DEXED_NEON_MKI_KERNEL
    if (add)
        neon_mki_kernel<true, true>(output, input, phase0, freq, gain1, dgain);
    else
        neon_mki_kernel<true, false>(output, input, phase0, freq, gain1, dgain);
#else
    int32_t gain = gain1;
    int32_t phase = phase0;
    const int32_t *adder = add ? output : zeros;
    
    for (int i = 0; i < N; i++) {
        gain += dgain;
        int32_t y = mkiSin((phase+input[i]), gain);
        output[i] = y + adder[i];
        phase += freq;
    }
#endif
}

void EngineMkI::compute_pure(int32_t *output, int32_t phase0, int32_t freq,
                             int32_t gain1, int32_t gain2, bool add) {
    int32_t dgain = (gain2 - gain1 + (N >> 1)) >> LG_N;
#ifdef DEXED_NEON_MKI_KERNEL
    if (add)
        neon_mki_kernel<false, true>(output, nullptr, phase0, freq, gain1, dgain);
    else
        neon_mki_kernel<false, false>(output, nullptr, phase0, freq, gain1, dgain);
#else
    int32_t gain = gain1;
    int32_t phase = phase0;
    const int32_t *adder = add ? output : zeros;
    
    for (int i = 0; i < N; i++) {
        gain += dgain;
        int32_t y = mkiSin(phase , gain);
        output[i] = y + adder[i];
        phase += freq;
    }
#endif
}

void EngineMkI::compute_fb(int32_t *output, int32_t phase0, int32_t freq,
                           int32_t gain1, int32_t gain2,
                           int32_t *fb_buf, int fb_shift, bool add) {
    int32_t dgain = (gain2 - gain1 + (N >> 1)) >> LG_N;
    int32_t gain = gain1;
    int32_t phase = phase0;
    const int32_t *adder = add ? output : zeros;
    int32_t y0 = fb_buf[0];
    int32_t y = fb_buf[1];
    
    for (int i = 0; i < N; i++) {
        gain += dgain;
        int32_t scaled_fb = (y0 + y) >> (fb_shift + 1);
        y0 = y;
        y = mkiSin((phase+scaled_fb), gain);
        output[i] = y + adder[i];
        phase += freq;
    }
    
    fb_buf[0] = y0;
    fb_buf[1] = y;
}

// exclusively used for ALGO 6 with feedback
void EngineMkI::compute_fb2(int32_t *output, FmOpParams *parms, int32_t gain01, int32_t gain02, int32_t *fb_buf, int fb_shift) {
    int32_t dgain[2];
    int32_t gain[2];
    int32_t phase[2];
    int32_t y0 = fb_buf[0];
    int32_t y = fb_buf[1];
    
    phase[0] = parms[0].phase;
    phase[1] = parms[1].phase;

    parms[1].gain_out = (ENV_MAX-(parms[1].level_in >> (28-ENV_BITDEPTH)));

    gain[0] = gain01;
    gain[1] = parms[1].gain_out == 0 ? (ENV_MAX-1) : parms[1].gain_out;

    dgain[0] = (gain02 - gain01 + (N >> 1)) >> LG_N;
    dgain[1] = (parms[1].gain_out - (parms[1].gain_out == 0 ? (ENV_MAX-1) : parms[1].gain_out));
    
    for (int i = 0; i < N; i++) {
        int32_t scaled_fb = (y0 + y) >> (fb_shift + 1);
        
        // op 0
        gain[0] += dgain[0];
        y0 = y;
        y = mkiSin(phase[0]+scaled_fb, gain[0]);
        phase[0] += parms[0].freq;
        
        // op 1
        gain[1] += dgain[1];
        y = mkiSin(phase[1]+y, gain[1]);
        phase[1] += parms[1].freq;
        
        output[i] = y;
    }
    fb_buf[0] = y0;
    fb_buf[1] = y;
}

// exclusively used for ALGO 4 with feedback
void EngineMkI::compute_fb3(int32_t *output, FmOpParams *parms, int32_t gain01, int32_t gain02, int32_t *fb_buf, int fb_shift) {
    int32_t dgain[3];
    int32_t gain[3];
    int32_t phase[3];
    int32_t y0 = fb_buf[0];
    int32_t y = fb_buf[1];
    
    phase[0] = parms[0].phase;
    phase[1] = parms[1].phase;
    phase[2] = parms[2].phase;
    
    parms[1].gain_out = (ENV_MAX-(parms[1].level_in >> (28-ENV_BITDEPTH)));
    parms[2].gain_out = (ENV_MAX-(parms[2].level_in >> (28-ENV_BITDEPTH)));
    
    gain[0] = gain01;
    gain[1] = parms[1].gain_out == 0 ? (ENV_MAX-1) : parms[1].gain_out;
    gain[2] = parms[2].gain_out == 0 ? (ENV_MAX-1) : parms[2].gain_out;

    dgain[0] = (gain02 - gain01 + (N >> 1)) >> LG_N;
    dgain[1] = (parms[1].gain_out - (parms[1].gain_out == 0 ? (ENV_MAX-1) : parms[1].gain_out));
    dgain[2] = (parms[2].gain_out - (parms[2].gain_out == 0 ? (ENV_MAX-1) : parms[2].gain_out));
    
    
    for (int i = 0; i < N; i++) {
        int32_t scaled_fb = (y0 + y) >> (fb_shift + 1);
        
        // op 0
        gain[0] += dgain[0];
        y0 = y;
        y = mkiSin(phase[0]+scaled_fb, gain[0]);
        phase[0] += parms[0].freq;
        
        // op 1
        gain[1] += dgain[1];
        y = mkiSin(phase[1]+y, gain[1]);
        phase[1] += parms[1].freq;
        
        // op 2
        gain[2] += dgain[2];
        y = mkiSin(phase[2]+y, gain[2]);
        phase[2] += parms[2].freq;
        
        output[i] = y;
    }
    fb_buf[0] = y0;
    fb_buf[1] = y;
}

void EngineMkI::render(int32_t *output, FmOpParams *params, int algorithm, int32_t *fb_buf, int32_t feedback_shift) {
    const int kLevelThresh = ENV_MAX-100;
    FmAlgorithm alg = algorithms[algorithm];
    bool has_contents[3] = { true, false, false };
    bool fb_on = feedback_shift < 16;

    switch(algorithm) {
        case 3 : case 5 :
            if ( fb_on )
                alg.ops[0] = 0xc4;
    }
    
    for (int op = 0; op < 6; op++) {
        int flags = alg.ops[op];
        bool add = (flags & OUT_BUS_ADD) != 0;
        FmOpParams &param = params[op];
        int inbus = (flags >> 4) & 3;
        int outbus = flags & 3;
        int32_t *outptr = (outbus == 0) ? output : buf_[outbus - 1].get();
        int32_t gain1 = param.gain_out == 0 ? (ENV_MAX-1) : param.gain_out;
        int32_t gain2 = ENV_MAX-(param.level_in >> (28-ENV_BITDEPTH));
        param.gain_out = gain2;
        
        if (gain1 <= kLevelThresh || gain2 <= kLevelThresh) {
            
            if (!has_contents[outbus]) {
                add = false;
            }
            
            if (inbus == 0 || !has_contents[inbus]) {
                // PG: this is my 'dirty' implementation of FB for 2 and 3 operators...
                if ((flags & 0xc0) == 0xc0 && fb_on) {
                    switch ( algorithm ) {
                        // three operator feedback, process exception for ALGO 4
                        case 3 :
                            compute_fb3(outptr, params, gain1, gain2, fb_buf, min((feedback_shift+2), 16));
                            params[1].phase += params[1].freq << LG_N; // hack, we already processed op-5 - op-4
                            params[2].phase += params[2].freq << LG_N; // yuk yuk
                            op += 2; // ignore the 2 other operators
                            break;
                        // two operator feedback, process exception for ALGO 6
                        case 5 :
                            compute_fb2(outptr, params, gain1, gain2, fb_buf, min((feedback_shift+2), 16));
                            params[1].phase += params[1].freq << LG_N;  // yuk, hack, we already processed op-5
                            op++; // ignore next operator;
                            break;
                        case 31 :
                            // one operator feedback, process exception for ALGO 32
                            compute_fb(outptr, param.phase, param.freq, gain1, gain2, fb_buf, min((feedback_shift+2), 16), add);
                            break;
                        default:
                            // one operator feedback, normal process
                            compute_fb(outptr, param.phase, param.freq, gain1, gain2, fb_buf, feedback_shift, add);
                            break;
                    }
                } else {
                    compute_pure(outptr, param.phase, param.freq, gain1, gain2, add);
                }
            } else {
                compute(outptr, buf_[inbus - 1].get(), param.phase, param.freq, gain1, gain2, add);
            }
            
            has_contents[outbus] = true;
        } else if (!add) {
            has_contents[outbus] = false;
        }
        param.phase += param.freq << LG_N;
    }
}

