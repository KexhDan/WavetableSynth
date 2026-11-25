#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Wavetable.h"

#include <array>
#include <atomic>

namespace WTSynth
{
    enum ModTarget
    {
        None = 0,
        Osc1WTPos,
        Osc1Pitch,
        Osc1Level,
        Osc1Pan,
        Osc1Unison,
        Osc2WTPos,
        Osc2Pitch,
        Osc2Level,
        Osc2Pan,
        Osc2Unison,
        FilterCutoff,
        NumTargets
    };

    static const char* const targetNames[NumTargets] =
    {
        "---",
        "OSC 1  WT Position",
        "OSC 1  Pitch",
        "OSC 1  Level",
        "OSC 1  Pan",
        "OSC 1  Unison Detune",
        "OSC 2  WT Position",
        "OSC 2  Pitch",
        "OSC 2  Level",
        "OSC 2  Pan",
        "OSC 2  Unison Detune",
        "Filtro  Cutoff"
    };

    static constexpr int numSlots   = 4;
    static constexpr int maxVoices  = 8;
    static constexpr int maxUnison  = 8;
    static constexpr int numOscs    = 2;
}

class WavetableSynthAudioProcessor : public juce::AudioProcessor
{
public:
    WavetableSynthAudioProcessor();
    ~WavetableSynthAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override                            {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannels()  == 0
            && layouts.getMainOutputChannels() == 2;
    }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                              { return true; }

    const juce::String getName() const override                  { return "Dual Wavetable Synth"; }
    bool acceptsMidi() const override                            { return true; }
    bool producesMidi() const override                           { return false; }
    bool supportsMidiProgramChange() const override              { return false; }
    double getTailLengthSeconds() const override                 { return 0.0; }

    int getNumPrograms() override                                { return 1; }
    int getCurrentProgram() override                             { return 0; }
    void setCurrentProgram (int) override                        {}
    const juce::String getProgramName (int) override             { return {}; }
    void changeProgramName (int, const juce::String&) override   {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    bool loadWavetable (int osc, const juce::File& file);
    Wavetable::Ptr getWavetable (int osc) const;
    juce::String getWavetableFileName (int osc) const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct Envelope
    {
        enum class Stage { Idle, Attack, Decay, Sustain, Release };

        Stage stage = Stage::Idle;
        float value = 0.0f;

        void noteOn()  { stage = Stage::Attack; }
        void noteOff() { if (stage != Stage::Idle && stage != Stage::Release) stage = Stage::Release; }

        float next (float a, float d, float s, float r, float sr) noexcept
        {
            switch (stage)
            {
                case Stage::Attack:
                    value += 1.0f / juce::jmax (0.001f, a) / sr;
                    if (value >= 1.0f) { value = 1.0f; stage = Stage::Decay; }
                    break;
                case Stage::Decay:
                    value -= (1.0f - s) / juce::jmax (0.001f, d) / sr;
                    if (value <= s) { value = s; stage = Stage::Sustain; }
                    break;
                case Stage::Sustain:
                    value = s;
                    break;
                case Stage::Release:
                    value -= 1.0f / juce::jmax (0.001f, r) / sr;
                    if (value <= 0.0f) { value = 0.0f; stage = Stage::Idle; }
                    break;
                case Stage::Idle:
                    value = 0.0f;
                    break;
            }
            return value;
        }

        bool isActive() const noexcept { return stage != Stage::Idle; }
    };

    struct Voice
    {
        bool active = false;
        int note = -1;
        float envValue = 0.0f;
        std::array<std::array<double, WTSynth::maxUnison>, WTSynth::numOscs> phase {};
        Envelope env;
    };

    struct Params
    {
        std::atomic<float>* master = nullptr;

        std::atomic<float>* wt[WTSynth::numOscs]      { nullptr, nullptr };
        std::atomic<float>* oct[WTSynth::numOscs]     { nullptr, nullptr };
        std::atomic<float>* semi[WTSynth::numOscs]    { nullptr, nullptr };
        std::atomic<float>* fine[WTSynth::numOscs]    { nullptr, nullptr };
        std::atomic<float>* unison[WTSynth::numOscs]  { nullptr, nullptr };
        std::atomic<float>* spread[WTSynth::numOscs]  { nullptr, nullptr };
        std::atomic<float>* level[WTSynth::numOscs]   { nullptr, nullptr };
        std::atomic<float>* pan[WTSynth::numOscs]     { nullptr, nullptr };

        std::atomic<float>* attack = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* sustain = nullptr;
        std::atomic<float>* release = nullptr;

        std::atomic<float>* cutoff = nullptr;
        std::atomic<float>* reso = nullptr;

        std::atomic<float>* lfoShape = nullptr;
        std::atomic<float>* lfoRate = nullptr;

        std::atomic<float>* modTarget[WTSynth::numSlots] { nullptr, nullptr, nullptr, nullptr };
        std::atomic<float>* modAmount[WTSynth::numSlots] { nullptr, nullptr, nullptr, nullptr };
    };

    struct ModState
    {
        float wt[WTSynth::numOscs]      { 0.0f, 0.0f };
        float pitch[WTSynth::numOscs]   { 0.0f, 0.0f };
        float level[WTSynth::numOscs]   { 0.0f, 0.0f };
        float pan[WTSynth::numOscs]     { 0.0f, 0.0f };
        float spread[WTSynth::numOscs]  { 0.0f, 0.0f };
        float cutoff = 0.0f;
    };

    void noteOn (int note);
    void noteOff (int note);

    Params params;

    std::array<Voice, WTSynth::maxVoices> voices;

    double sampleRate = 44100.0;
    double lfoPhase = 0.0;

    juce::dsp::StateVariableTPTFilter<float> filters[2];

    mutable juce::CriticalSection wtLock;
    Wavetable::Ptr wavetables[WTSynth::numOscs];
    juce::String wavPaths[WTSynth::numOscs];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavetableSynthAudioProcessor)
};
