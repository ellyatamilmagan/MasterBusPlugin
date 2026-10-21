#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

namespace ids
{
    inline constexpr const char* inGain     = "inGain";
    inline constexpr const char* lowEnd     = "lowEnd";
    inline constexpr const char* saturation = "saturation";
    inline constexpr const char* limiter    = "limiter";
}

//==============================================================================
// True brickwall: gain-min window -> moving average -> release smoothing.
// Audio is delayed by (window - 1) samples, which guarantees the output never
// exceeds the ceiling. Reported to the host as plugin latency.
class LookaheadLimiter
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();
    void setCeiling (float linearCeiling) noexcept   { ceiling = linearCeiling; }
    int  getLatency() const noexcept                 { return window - 1; }
    void process (juce::AudioBuffer<float>& buffer);

private:
    juce::AudioBuffer<float> delay;
    std::vector<float> tHist, mHist;
    double sumM = 0.0;
    float  env = 1.0f, ceiling = 1.0f, relCoeff = 0.0f;
    int    window = 2, pos = 0, dpos = 0;
};

//==============================================================================
class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                     { return true; }

    const juce::String getName() const override        { return "MasterBus"; }
    bool acceptsMidi() const override                   { return false; }
    bool producesMidi() const override                  { return false; }
    double getTailLengthSeconds() const override        { return 0.0; }

    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void processChunk (juce::AudioBuffer<float>&);

    std::atomic<float> *pIn = nullptr, *pLow = nullptr, *pSat = nullptr, *pCeil = nullptr;

    juce::SmoothedValue<float> gainSm, lowSm, satSm;
    juce::dsp::LinkwitzRileyFilter<float> xover;      // LR4 @ 120 Hz
    juce::dsp::Oversampling<float> oversampler;       // 2x around the saturator
    LookaheadLimiter limiter;
    int maxBlock = 512;

    static constexpr float crossoverHz = 120.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
