// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#include "FxPlots.h"
#include "ui/DXLookNFeel.h"

#include <complex>

using namespace fx;

namespace {
// As Dexed's displays (GlobalEditor_1728x288.png)
const Colour displayColour { 0xff141212 };
const Colour gridColour { 0x1affffff };
const Colour textColour { 0x80ffffff };
const Colour meterColour { 0xffe0a030 };

void drawLabel(Graphics& g, Rectangle<float> area, const String& text, Justification justification) {
    g.setColour(textColour);
    g.setFont(FontOptions(9.5f));
    g.drawText(text, area.reduced(3.0f, 1.0f), justification, false);
}

void strokeCurve(Graphics& g, const Path& path) {
    g.setColour(DXLookNFeel::fillColour.brighter(0.3f));
    g.strokePath(path, PathStrokeType(1.5f, PathStrokeType::curved, PathStrokeType::rounded));
}

// Fills a curve down to a baseline, then strokes it
void drawFilledCurve(Graphics& g, const Path& curve, Rectangle<float> area, float baselineY) {
    Path fill(curve);
    fill.lineTo(area.getRight(), baselineY);
    fill.lineTo(area.getX(), baselineY);
    fill.closeSubPath();
    g.setColour(DXLookNFeel::fillColour.withAlpha(0.2f));
    g.fillPath(fill);
    strokeCurve(g, curve);
}

void drawDot(Graphics& g, Point<float> centre) {
    g.setColour(Colours::white);
    g.fillEllipse(Rectangle<float>(6.0f, 6.0f).withCentre(centre));
}

//==============================================================================
/** Output vs input amplitude (-1 to 1), with a dot at the current input peak. */
class DistortionPlot : public FxPlot {
public:
    using FxPlot::FxPlot;

    void paintPlot(Graphics& g, Rectangle<float> area) override {
        const float driveGain = Decibels::decibelsToGain(get(distortionDrive));
        const float shapeValue = get(distortionShape);
        const float outputGain = FxChain::getDistortionOutputGain(driveGain);
        const auto mix = FxChain::getMix(fx::distortion, get(distortionMix));
        auto transfer = [&](float x) {
            return mix.dry * x + mix.wet * FxChain::shape(x * driveGain, shapeValue) * outputGain;
        };

        const auto plot = area.reduced(4.0f, 3.0f);
        auto toPoint = [&](float x, float y) {
            return Point<float>(plot.getX() + (x + 1.0f) * 0.5f * plot.getWidth(),
                                plot.getCentreY() - jlimit(-1.05f, 1.05f, y) * 0.5f * plot.getHeight());
        };

        g.setColour(gridColour);
        g.drawHorizontalLine(roundToInt(plot.getCentreY()), plot.getX(), plot.getRight());
        g.drawVerticalLine(roundToInt(plot.getCentreX()), plot.getY(), plot.getBottom());
        g.drawLine(Line<float>(toPoint(-1.0f, -1.0f), toPoint(1.0f, 1.0f)), 1.0f);

        Path curve;
        const int numPoints = roundToInt(plot.getWidth());
        for (int i = 0; i <= numPoints; ++i) {
            const float x = -1.0f + 2.0f * (float) i / (float) numPoints;
            const auto point = toPoint(x, transfer(x));
            i == 0 ? curve.startNewSubPath(point) : curve.lineTo(point);
        }
        strokeCurve(g, curve);

        if (live.distortionInput > 0.001f) {
            const float x = jmin(1.0f, live.distortionInput);
            drawDot(g, toPoint(x, transfer(x)));
        }
        drawLabel(g, area, "in/out", Justification::topLeft);
    }
};

//==============================================================================
/** Magnitude response (dB) from 20 Hz to 20 kHz. */
class FilterPlot : public FxPlot {
public:
    using FxPlot::FxPlot;

    void paintPlot(Graphics& g, Rectangle<float> area) override {
        constexpr float topDb = 24.0f, bottomDb = -36.0f;
        const float cutoff = get(filterCutoff);
        const float k = 1.0f / get(filterResonance);
        const float type = get(filterType);
        const float lowPassGain = jmax(0.0f, 1.0f - 2.0f * type);
        const float highPassGain = jmax(0.0f, 2.0f * type - 1.0f);
        const float bandPassGain = 1.0f - lowPassGain - highPassGain;
        const auto mix = FxChain::getMix(fx::filter, get(filterMix));

        auto toX = [&](float hz) { return area.getX() + std::log(hz / 20.0f) / std::log(1000.0f) * area.getWidth(); };
        auto toY = [&](float db) { return area.getY() + (topDb - db) / (topDb - bottomDb) * area.getHeight(); };

        g.setColour(gridColour);
        g.drawHorizontalLine(roundToInt(toY(0.0f)), area.getX(), area.getRight());
        for (float hz : { 100.0f, 1000.0f, 10000.0f })
            g.drawVerticalLine(roundToInt(toX(hz)), area.getY(), area.getBottom());

        // Analog prototype of the state variable filter (s normalised to the cutoff)
        Path curve;
        const int numPoints = roundToInt(area.getWidth());
        for (int i = 0; i <= numPoints; ++i) {
            const float hz = 20.0f * std::pow(1000.0f, (float) i / (float) numPoints);
            const float w = hz / cutoff;
            const std::complex<float> denominator(1.0f - w * w, k * w);
            const std::complex<float> response = (lowPassGain + bandPassGain * std::complex<float>(0.0f, k * w)
                                                  - highPassGain * w * w) / denominator;
            const float db = Decibels::gainToDecibels(std::abs(mix.dry + mix.wet * response), bottomDb - 10.0f);
            const Point<float> point(area.getX() + (float) i, toY(jlimit(bottomDb - 2.0f, topDb, db)));
            i == 0 ? curve.startNewSubPath(point) : curve.lineTo(point);
        }
        drawFilledCurve(g, curve, area, area.getBottom());
    }
};

//==============================================================================
/** Output vs input level (dB), with the current input and gain reduction. */
class CompressorPlot : public FxPlot {
public:
    using FxPlot::FxPlot;

    void paintPlot(Graphics& g, Rectangle<float> area) override {
        constexpr float minDb = -60.0f, maxOutDb = 6.0f;
        constexpr float maxReductionDb = 24.0f;
        const float threshold = get(compressorThreshold);
        const float ratio = get(compressorRatio);
        const float makeUp = FxChain::getCompressorMakeUpDecibels(threshold, ratio);
        const auto mix = FxChain::getMix(fx::compressor, get(compressorMix));
        auto transfer = [&](float inDb) {
            const float compressed = (inDb <= threshold ? inDb : threshold + (inDb - threshold) / ratio) + makeUp;
            return Decibels::gainToDecibels(mix.dry * Decibels::decibelsToGain(inDb)
                                            + mix.wet * Decibels::decibelsToGain(compressed), -100.0f);
        };

        auto meterArea = area.reduced(0.0f, 3.0f);
        const auto reductionBar = meterArea.removeFromRight(5.0f);
        const auto plot = meterArea.withTrimmedRight(4.0f);
        auto toPoint = [&](float inDb, float outDb) {
            return Point<float>(plot.getX() + (inDb - minDb) / -minDb * plot.getWidth(),
                                plot.getY() + (maxOutDb - jlimit(minDb, maxOutDb, outDb)) / (maxOutDb - minDb) * plot.getHeight());
        };

        g.setColour(gridColour);
        g.drawLine(Line<float>(toPoint(minDb, minDb), toPoint(0.0f, 0.0f)), 1.0f);
        const float thresholdX = toPoint(threshold, 0.0f).x;
        const float dashes[] = { 2.0f, 2.0f };
        g.drawDashedLine(Line<float>(thresholdX, plot.getY(), thresholdX, plot.getBottom()), dashes, 2);

        Path curve;
        const int numPoints = roundToInt(plot.getWidth());
        for (int i = 0; i <= numPoints; ++i) {
            const float inDb = minDb - minDb * (float) i / (float) numPoints;
            const auto point = toPoint(inDb, transfer(inDb));
            i == 0 ? curve.startNewSubPath(point) : curve.lineTo(point);
        }
        strokeCurve(g, curve);

        const float inputDb = Decibels::gainToDecibels(live.compressorInput, -100.0f);
        if (inputDb > minDb)
            drawDot(g, toPoint(jmin(0.0f, inputDb), transfer(jmin(0.0f, inputDb))));

        // Gain reduction meter (from the top)
        g.setColour(gridColour);
        g.fillRect(reductionBar);
        g.setColour(meterColour);
        g.fillRect(reductionBar.withHeight(jmin(1.0f, live.compressorGainReduction / maxReductionDb) * reductionBar.getHeight()));
        if (live.compressorGainReduction >= 0.5f)
            drawLabel(g, area.withTrimmedRight(9.0f), "-" + String(live.compressorGainReduction, 1) + " dB",
                      Justification::bottomRight);
    }
};

//==============================================================================
/** The LFO over the last 2 seconds (moves while sound plays). */
class ModulationPlot : public FxPlot {
public:
    using FxPlot::FxPlot;

    void paintPlot(Graphics& g, Rectangle<float> area) override {
        constexpr double windowSeconds = 2.0;
        const double rate = get(modulationRate);
        const float depth = get(modulationDepth);
        const auto plot = area.reduced(4.0f, 6.0f);

        g.setColour(gridColour);
        g.drawHorizontalLine(roundToInt(plot.getCentreY()), plot.getX(), plot.getRight());

        Path curve;
        const int numPoints = roundToInt(plot.getWidth());
        Point<float> point;
        for (int i = 0; i <= numPoints; ++i) {
            const double secondsAgo = windowSeconds * (1.0 - (double) i / (double) numPoints);
            const double phase = live.modulationPhase - rate * secondsAgo;
            point = { plot.getX() + (float) i,
                      plot.getCentreY() - depth * (float) std::sin(MathConstants<double>::twoPi * phase) * 0.5f * plot.getHeight() };
            i == 0 ? curve.startNewSubPath(point) : curve.lineTo(point);
        }
        strokeCurve(g, curve);
        drawDot(g, point);

        drawLabel(g, area, getParamInfo(modulationCharacter).toText(get(modulationCharacter)), Justification::topLeft);
    }
};

//==============================================================================
/** The echoes of an impulse: left channel up, right channel down. */
class DelayPlot : public FxPlot {
public:
    using FxPlot::FxPlot;

    void paintPlot(Graphics& g, Rectangle<float> area) override {
        constexpr int maxEchoes = 24;
        const float time = get(delayTime) * 0.001f;
        const float feedback = get(delayFeedback);
        const float spread = get(delaySpread);
        const auto mix = FxChain::getMix(fx::delay, get(delayMix));
        // (later echoes are darker, as the tone filter takes their highs)
        const float toneFade = 0.55f + 0.45f * toNormalised(delayTone, get(delayTone));

        // Same routing as FxChain::processDelay(), for an impulse in both channels
        std::vector<std::pair<float, float>> echoes;
        float left = 1.0f, right = 1.0f - spread;
        while ((int) echoes.size() < maxEchoes && jmax(left, right) >= 0.02f) {
            echoes.emplace_back(left, right);
            const float nextLeft = feedback * (left + (right - left) * spread);
            right = feedback * (right + (left - right) * spread);
            left = nextLeft;
        }
        const float window = jmax(0.25f, time * (float) (echoes.size() + 1));

        const auto plot = area.reduced(4.0f, 3.0f);
        const float halfHeight = plot.getHeight() * 0.5f - 1.0f;
        auto toX = [&](float t) { return plot.getX() + t / window * plot.getWidth(); };

        g.setColour(gridColour);
        g.drawHorizontalLine(roundToInt(plot.getCentreY()), plot.getX(), plot.getRight());

        auto drawStem = [&](float t, float up, float down) {
            const float x = toX(t);
            g.fillRect(Rectangle<float>(x - 1.0f, plot.getCentreY() - up * halfHeight, 2.0f, up * halfHeight));
            g.fillRect(Rectangle<float>(x - 1.0f, plot.getCentreY(), 2.0f, down * halfHeight));
        };
        g.setColour(Colours::white.withAlpha(0.6f));
        drawStem(0.0f, mix.dry, mix.dry);
        float alpha = 1.0f;
        for (size_t n = 0; n < echoes.size(); ++n) {
            g.setColour(DXLookNFeel::fillColour.brighter(0.3f).withAlpha(alpha));
            drawStem(time * (float) (n + 1), echoes[n].first * mix.wet, echoes[n].second * mix.wet);
            alpha *= toneFade;
        }

        drawLabel(g, area, window < 1.0f ? String(roundToInt(window * 1000.0f)) + " ms" : String(window, 1) + " s",
                  Justification::bottomRight);
    }
};

//==============================================================================
/**
 * The decay of the reverb (dB, linear in time): low frequencies, and high
 * frequencies (shorter, as damping takes them). Left channel up, right channel
 * down: the more width, the more they differ.
 */
class ReverbPlot : public FxPlot {
public:
    using FxPlot::FxPlot;

    void paintPlot(Graphics& g, Rectangle<float> area) override {
        // juce::Reverb (Freeverb): feedback of its comb filters, their mean
        // length (at 44.1 kHz), and the one-pole low-pass in their loop
        constexpr float meanCombSeconds = 1378.0f / 44100.0f;
        const float feedback = get(reverbSize) * 0.28f + 0.7f;
        const float damp = get(reverbDamping) * 0.4f;
        const float highFeedback = feedback * (1.0f - damp) / (1.0f + damp); // (at Nyquist)
        auto getRt60 = [](float loopGain) { return meanCombSeconds * 3.0f / -std::log10(loopGain); };
        const float lowRt60 = getRt60(feedback);
        const float highRt60 = getRt60(highFeedback);
        const float width = get(reverbWidth);
        const auto mix = FxChain::getMix(fx::reverb, get(reverbMix));
        const float window = lowRt60 * 1.1f;

        const auto plot = area.reduced(4.0f, 3.0f);
        const float halfHeight = plot.getHeight() * 0.5f - 1.0f;
        g.setColour(gridColour);
        g.drawHorizontalLine(roundToInt(plot.getCentreY()), plot.getX(), plot.getRight());

        static const auto noise = [] {
            Random random(1234);
            std::array<std::pair<float, float>, 256> values;
            for (auto& value : values)
                value = { 0.55f + 0.45f * random.nextFloat(), 0.55f + 0.45f * random.nextFloat() };
            return values;
        }();

        auto drawTail = [&](float rt60, Colour colour) {
            g.setColour(colour);
            int stripe = 0;
            for (float x = 3.0f; x < plot.getWidth(); x += 2.0f, ++stripe) {
                const float level = mix.wet * jmax(0.0f, 1.0f - x / plot.getWidth() * window / rt60);
                if (level <= 0.0f)
                    break;
                const auto [noise1, noise2] = noise[(size_t) stripe % noise.size()];
                const float up = level * noise1;
                const float down = level * (noise1 + (noise2 - noise1) * width);
                g.fillRect(Rectangle<float>(plot.getX() + x, plot.getCentreY() - up * halfHeight, 1.0f, (up + down) * halfHeight));
            }
        };
        drawTail(lowRt60, DXLookNFeel::fillColour.withAlpha(0.45f));
        drawTail(highRt60, DXLookNFeel::fillColour.brighter(0.4f));

        g.setColour(Colours::white.withAlpha(0.6f));
        g.fillRect(Rectangle<float>(plot.getX(), plot.getCentreY() - mix.dry * halfHeight, 2.0f, 2.0f * mix.dry * halfHeight));

        drawLabel(g, area, "RT60 " + String(lowRt60, 1) + " s", Justification::topRight);
    }
};
} // namespace

//==============================================================================
void FxPlot::paint(Graphics& g) {
    const auto area = getLocalBounds().toFloat();
    g.setColour(displayColour);
    g.fillRoundedRectangle(area, 3.0f);

    Graphics::ScopedSaveState saveState(g);
    g.reduceClipRegion(getLocalBounds().reduced(1));
    if (module.hasBypassedLook()) {
        g.beginTransparencyLayer(0.35f);
        paintPlot(g, area);
        g.endTransparencyLayer();
    } else {
        paintPlot(g, area);
    }
}

std::unique_ptr<FxPlot> FxPlot::create(int module, const FxModule& owner, const FxLiveData& liveData) {
    switch (module) {
        case fx::distortion: return std::make_unique<DistortionPlot>(owner, liveData);
        case fx::filter: return std::make_unique<FilterPlot>(owner, liveData);
        case fx::compressor: return std::make_unique<CompressorPlot>(owner, liveData);
        case fx::modulation: return std::make_unique<ModulationPlot>(owner, liveData);
        case fx::delay: return std::make_unique<DelayPlot>(owner, liveData);
        case fx::reverb: return std::make_unique<ReverbPlot>(owner, liveData);
        default: return nullptr;
    }
}
