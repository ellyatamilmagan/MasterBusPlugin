#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

//==============================================================================
// LookaheadLimiter
//==============================================================================
void LookaheadLimiter::prepare (double sampleRate, int numChannels)
{
    window   = juce::jmax (2, juce::roundToInt (0.0015 * sampleRate));          // 1.5 ms lookahead
    relCoeff = 1.0f - std::exp (-1.0f / (0.1f * (float) sampleRate));           // 100 ms release
    delay.setSize (numChannels, window - 1);
    tHist.assign ((size_t) window, 1.0f);
    mHist.assign ((size_t) window, 1.0f);
    reset();
}

void LookaheadLimiter::reset()
{
    delay.clear();
    std::fill (tHist.begin(), tHist.end(), 1.0f);
    std::fill (mHist.begin(), mHist.end(), 1.0f);
    sumM = (double) window;
    env = 1.0f;
    pos = dpos = 0;
}

void LookaheadLimiter::process (juce::AudioBuffer<float>& buffer)
{
    const int numCh = juce::jmin (buffer.getNumChannels(), delay.getNumChannels());
    const int D = window - 1;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));

        tHist[(size_t) pos] = peak > ceiling ? ceiling / peak : 1.0f;
        const float m = *std::min_element (tHist.begin(), tHist.end());

        sumM += (double) m - (double) mHist[(size_t) pos];
        mHist[(size_t) pos] = m;

        const float avg = (float) (sumM / window);
        env = avg < env ? avg : env + relCoeff * (avg - env);   // instant attack, smooth release

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float out = delay.getSample (ch, dpos);
            delay.setSample (ch, dpos, buffer.getSample (ch, i));
            buffer.setSample (ch, i, juce::jlimit (-ceiling, ceiling, out * env)); // final safety clamp
        }

        pos  = (pos + 1) % window;
        dpos = (dpos + 1) % D;
    }
}

//==============================================================================
// PluginProcessor
//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout()),
      oversampler (2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true)
{
    pIn   = apvts.getRawParameterValue (ids::inGain);
    pLow  = apvts.getRawParameterValue (ids::lowEnd);
    pSat  = apvts.getRawParameterValue (ids::saturation);
    pCeil = apvts.getRawParameterValue (ids::limiter);
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::inGain, 1 }, "Input Gain",
                    NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
                    AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::lowEnd, 1 }, "Low-End Collision Control",
                    NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
                    AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::saturation, 1 }, "Saturation",
                    NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
                    AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::limiter, 1 }, "Brickwall Limiter",
                    NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -0.3f,
                    AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = juce::jmax (1, samplesPerBlock);

    xover.prepare ({ sampleRate, (juce::uint32) maxBlock, 2 });
    xover.setCutoffFrequency (crossoverHz);
    xover.reset();

    oversampler.initProcessing ((size_t) maxBlock);
    oversampler.reset();

    limiter.prepare (sampleRate, 2);

    gainSm.reset (sampleRate, 0.02);  gainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pIn->load()));
    lowSm .reset (sampleRate, 0.02);  lowSm .setCurrentAndTargetValue (pLow->load() * 0.01f);
    satSm .reset (sampleRate, 0.02);  satSm .setCurrentAndTargetValue (pSat->load() * 0.01f);

    setLatencySamples (juce::roundToInt (oversampler.getLatencyInSamples()) + limiter.getLatency());
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (buffer.getNumChannels() < 2)
        return;

    // Chunking guards against hosts delivering blocks larger than promised.
    for (int start = 0; start < buffer.getNumSamples(); start += maxBlock)
    {
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), 2, start,
                                        juce::jmin (maxBlock, buffer.getNumSamples() - start));
        processChunk (chunk);
    }
}

void PluginProcessor::processChunk (juce::AudioBuffer<float>& buf)
{
    const int n = buf.getNumSamples();

    gainSm.setTargetValue (juce::Decibels::decibelsToGain (pIn->load()));
    lowSm .setTargetValue (pLow->load() * 0.01f);
    satSm .setTargetValue (pSat->load() * 0.01f);
    limiter.setCeiling (juce::Decibels::decibelsToGain (pCeil->load()));

    auto* l = buf.getWritePointer (0);
    auto* r = buf.getWritePointer (1);

    // 1) Input gain + low-end collision control (mono-sum the band below crossover)
    for (int i = 0; i < n; ++i)
    {
        const float g   = gainSm.getNextValue();
        const float amt = lowSm.getNextValue();

        float lowL, highL, lowR, highR;
        xover.processSample (0, l[i] * g, lowL, highL);
        xover.processSample (1, r[i] * g, lowR, highR);

        const float mono = 0.5f * (lowL + lowR);
        l[i] = highL + lowL + amt * (mono - lowL);
        r[i] = highR + lowR + amt * (mono - lowR);
    }

    // 2) Saturation: normalised tanh, 2x oversampled, amount ramped across the block
    const float a0 = satSm.getCurrentValue();
    satSm.skip (n);
    const float a1 = satSm.getCurrentValue();

    juce::dsp::AudioBlock<float> block (buf);
    auto os = oversampler.processSamplesUp (block);
    const size_t osN = os.getNumSamples();

    for (size_t ch = 0; ch < 2; ++ch)
    {
        auto* p = os.getChannelPointer (ch);
        for (size_t i = 0; i < osN; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / (float) osN;
            const float d = 1.0f + 4.0f * a;
            p[i] += a * (std::tanh (d * p[i]) / std::tanh (d) - p[i]);
        }
    }
    oversampler.processSamplesDown (block);

    // 3) Brickwall limiter (always last)
    limiter.process (buf);
}

//==============================================================================
juce::AudioProcessorEditor* PluginProcessor::createEditor() { return new PluginEditor (*this); }

void PluginProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void PluginProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PluginProcessor(); }
