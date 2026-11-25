#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
WavetableSynthAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&layout] (const char* id, const char* name,
                               float min, float max, float def,
                               float skew = 1.0f, const juce::String& unit = "")
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> (min, max, 0.0f, skew),
            def, juce::AudioParameterFloatAttributes().withLabel (unit)));
    };

    auto addInt = [&layout] (const char* id, const char* name, int min, int max, int def)
    {
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { id, 1 }, name, min, max, def));
    };

    addFloat ("master", "Master", 0.0f, 1.0f, 0.7f);

    const char* oscNames[2] = { "1", "2" };
    const char* oscIds[2]   = { "o1", "o2" };

    for (int o = 0; o < 2; ++o)
    {
        const juce::String n = oscNames[o];
        const juce::String id = oscIds[o];

        addFloat ((id + "wt").toRawUTF8(),      ("OSC " + n + " WT Pos").toRawUTF8(),      0.0f, 1.0f, 0.0f);
        addFloat ((id + "oct").toRawUTF8(),     ("OSC " + n + " Octave").toRawUTF8(),    -2.0f, 2.0f, 0.0f);
        addFloat ((id + "semi").toRawUTF8(),    ("OSC " + n + " Semitone").toRawUTF8(), -12.0f, 12.0f, 0.0f);
        addFloat ((id + "fine").toRawUTF8(),    ("OSC " + n + " Fine").toRawUTF8(),    -100.0f, 100.0f, 0.0f, 1.0f, "ct");
        addInt   ((id + "unison").toRawUTF8(),  ("OSC " + n + " Unison").toRawUTF8(),   1, 8, 1);
        addFloat ((id + "spread").toRawUTF8(),  ("OSC " + n + " Spread").toRawUTF8(),    0.0f, 100.0f, 10.0f, 1.0f, "ct");
        addFloat ((id + "level").toRawUTF8(),   ("OSC " + n + " Level").toRawUTF8(),     0.0f, 1.0f, 0.8f);
        addFloat ((id + "pan").toRawUTF8(),     ("OSC " + n + " Pan").toRawUTF8(),      -1.0f, 1.0f, 0.0f);
    }

    addFloat ("attack",  "Attack",  0.001f, 2.0f, 0.01f, 0.35f, "s");
    addFloat ("decay",   "Decay",   0.001f, 2.0f, 0.25f, 0.35f, "s");
    addFloat ("sustain", "Sustain", 0.0f,   1.0f, 0.7f);
    addFloat ("release", "Release", 0.005f, 5.0f, 0.30f, 0.35f, "s");

    addFloat ("cutoff", "Cutoff", 20.0f, 20000.0f, 18000.0f, 0.30f, "Hz");
    addFloat ("reso",   "Reso",   0.1f,  20.0f,    0.7f);

    addInt ("lfoShape", "LFO Shape", 0, 3, 0);
    addFloat ("lfoRate", "LFO Rate", 0.05f, 20.0f, 2.0f, 0.4f, "Hz");

    for (int i = 0; i < WTSynth::numSlots; ++i)
    {
        addInt   (("m" + juce::String (i + 1) + "target").toRawUTF8(),
                  ("Mod " + juce::String (i + 1) + " Target").toRawUTF8(), 0, WTSynth::NumTargets - 1, 0);
        addFloat (("m" + juce::String (i + 1) + "amount").toRawUTF8(),
                  ("Mod " + juce::String (i + 1) + " Amount").toRawUTF8(), -1.0f, 1.0f, 0.0f);
    }

    return layout;
}

//==============================================================================
WavetableSynthAudioProcessor::WavetableSynthAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    auto bind = [this] (const juce::String& id) -> std::atomic<float>*
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };

    params.master = bind ("master");

    for (int o = 0; o < WTSynth::numOscs; ++o)
    {
        const juce::String id = (o == 0) ? "o1" : "o2";
        params.wt[o]     = bind (id + "wt");
        params.oct[o]    = bind (id + "oct");
        params.semi[o]   = bind (id + "semi");
        params.fine[o]   = bind (id + "fine");
        params.unison[o] = bind (id + "unison");
        params.spread[o] = bind (id + "spread");
        params.level[o]  = bind (id + "level");
        params.pan[o]    = bind (id + "pan");
    }

    params.attack  = bind ("attack");
    params.decay   = bind ("decay");
    params.sustain = bind ("sustain");
    params.release = bind ("release");

    params.cutoff = bind ("cutoff");
    params.reso   = bind ("reso");

    params.lfoShape = bind ("lfoShape");
    params.lfoRate  = bind ("lfoRate");

    for (int i = 0; i < WTSynth::numSlots; ++i)
    {
        params.modTarget[i] = bind ("m" + juce::String (i + 1) + "target");
        params.modAmount[i] = bind ("m" + juce::String (i + 1) + "amount");
    }
}

WavetableSynthAudioProcessor::~WavetableSynthAudioProcessor() = default;

//==============================================================================
void WavetableSynthAudioProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    lfoPhase = 0.0;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 2;

    for (auto& filter : filters)
    {
        filter.reset();
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.prepare (spec);
    }

    for (auto& voice : voices)
    {
        voice.active = false;
        voice.note = -1;
        voice.env.value = 0.0f;
        voice.env.stage = Envelope::Stage::Idle;
        voice.envValue = 0.0f;
    }
}

bool WavetableSynthAudioProcessor::loadWavetable (int osc, const juce::File& file)
{
    if (osc < 0 || osc >= WTSynth::numOscs)
        return false;

    Wavetable::Ptr newTable (new Wavetable());

    if (! newTable->loadFrom (file))
        return false;

    {
        juce::ScopedLock lock (wtLock);
        wavetables[osc] = newTable;
        wavPaths[osc] = file.getFullPathName();
    }

    return true;
}

Wavetable::Ptr WavetableSynthAudioProcessor::getWavetable (int osc) const
{
    juce::ScopedLock lock (wtLock);
    return (osc >= 0 && osc < WTSynth::numOscs) ? wavetables[osc] : nullptr;
}

juce::String WavetableSynthAudioProcessor::getWavetableFileName (int osc) const
{
    auto table = getWavetable (osc);
    return table != nullptr ? table->getFileName() : juce::String();
}

//==============================================================================
void WavetableSynthAudioProcessor::noteOn (int note)
{
    Voice* target = nullptr;

    for (auto& voice : voices)
    {
        if (! voice.active)
        {
            target = &voice;
            break;
        }
    }

    if (target == nullptr)
    {
        float lowest = std::numeric_limits<float>::max();

        for (auto& voice : voices)
        {
            if (voice.env.value < lowest)
            {
                lowest = voice.env.value;
                target = &voice;
            }
        }
    }

    if (target == nullptr)
        return;

    target->active = true;
    target->note = note;
    target->env.noteOn();

    for (int o = 0; o < WTSynth::numOscs; ++o)
        for (int u = 0; u < WTSynth::maxUnison; ++u)
            target->phase[o][u] = (double) u / (double) WTSynth::maxUnison
                                + (o == 1 ? 0.013 : 0.0);
}

void WavetableSynthAudioProcessor::noteOff (int note)
{
    for (auto& voice : voices)
        if (voice.active && voice.note == note)
            voice.env.noteOff();
}

//==============================================================================
void WavetableSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                 juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const float master = juce::jlimit (0.0f, 1.0f, params.master->load());

    // Copia los punteros shared de las wavetables una sola vez por bloque.
    Wavetable::Ptr tables[WTSynth::numOscs];
    {
        juce::ScopedLock lock (wtLock);
        tables[0] = wavetables[0];
        tables[1] = wavetables[1];
    }

    struct TimedMessage { int position; juce::MidiMessage message; };
    std::vector<TimedMessage> events;

    {
        juce::MidiBuffer::Iterator it (midiMessages);
        juce::MidiMessage msg;
        int pos = 0;
        while (it.getNextEvent (msg, pos))
            events.push_back ({ pos, msg });
    }

    midiMessages.clear(); // synth: no reenvía MIDI al host

    size_t eventIndex = 0;

    const float attack  = params.attack->load();
    const float decay   = params.decay->load();
    const float sustain = params.sustain->load();
    const float release = params.release->load();
    const float baseCutoff = params.cutoff->load();
    const float reso = params.reso->load();
    const float lfoRate = params.lfoRate->load();
    const int shape = (int) params.lfoShape->load();

    const float baseWt[2]     = { params.wt[0]->load(),     params.wt[1]->load() };
    const float baseOct[2]    = { params.oct[0]->load(),    params.oct[1]->load() };
    const float baseSemi[2]   = { params.semi[0]->load(),   params.semi[1]->load() };
    const float baseFine[2]   = { params.fine[0]->load(),   params.fine[1]->load() };
    const float baseUnison[2] = { params.unison[0]->load(), params.unison[1]->load() };
    const float baseSpread[2] = { params.spread[0]->load(), params.spread[1]->load() };
    const float baseLevel[2]  = { params.level[0]->load(),  params.level[1]->load() };
    const float basePan[2]    = { params.pan[0]->load(),    params.pan[1]->load() };

    const float maxCutoff = (float) juce::jmin (20000.0, sampleRate * 0.45);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        while (eventIndex < events.size() && events[eventIndex].position == sample)
        {
            const auto& msg = events[eventIndex].message;

            if (msg.isNoteOn())
                noteOn (msg.getNoteNumber());
            else if (msg.isNoteOff())
                noteOff (msg.getNoteNumber());
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                for (auto& voice : voices)
                    if (voice.active)
                        voice.env.noteOff();

            ++eventIndex;
        }

        //--- LFO -------------------------------------------------------------
        lfoPhase += lfoRate / sampleRate;
        if (lfoPhase >= 1.0)
            lfoPhase -= std::floor (lfoPhase);

        const float phase = (float) lfoPhase;
        float lfo = 0.0f;

        switch (shape)
        {
            case 0:  lfo = std::sin (juce::MathConstants<float>::twoPi * phase); break;
            case 1:  lfo = 1.0f - 4.0f * std::fabs (phase - 0.5f); break;
            case 2:  lfo = 2.0f * phase - 1.0f; break;
            default: lfo = phase < 0.5f ? 1.0f : -1.0f; break;
        }

        //--- Matriz de modulacion -------------------------------------------
        ModState mod;

        for (int i = 0; i < WTSynth::numSlots; ++i)
        {
            const int target = (int) params.modTarget[i]->load();
            if (target == WTSynth::None)
                continue;

            const float v = lfo * params.modAmount[i]->load();
            const int osc = (target >= WTSynth::Osc1WTPos && target <= WTSynth::Osc1Unison) ? 0 : 1;

            switch (target)
            {
                case WTSynth::Osc1WTPos: case WTSynth::Osc2WTPos:     mod.wt[osc] += v;      break;
                case WTSynth::Osc1Pitch: case WTSynth::Osc2Pitch:     mod.pitch[osc] += v;   break;
                case WTSynth::Osc1Level: case WTSynth::Osc2Level:     mod.level[osc] += v;   break;
                case WTSynth::Osc1Pan:   case WTSynth::Osc2Pan:       mod.pan[osc] += v;     break;
                case WTSynth::Osc1Unison: case WTSynth::Osc2Unison:   mod.spread[osc] += v;  break;
                case WTSynth::FilterCutoff:                            mod.cutoff += v;       break;
                default: break;
            }
        }

        //--- Envolvente (una sola vez por voz y sample) ----------------------
        for (auto& voice : voices)
        {
            if (! voice.active)
                continue;

            voice.envValue = voice.env.next (attack, decay, sustain, release, (float) sampleRate);

            if (! voice.env.isActive())
            {
                voice.active = false;
                voice.note = -1;
                voice.envValue = 0.0f;
            }
        }

        float left = 0.0f;
        float right = 0.0f;

        //--- Osciladores -----------------------------------------------------
        for (int o = 0; o < WTSynth::numOscs; ++o)
        {
            const auto& table = tables[o];

            if (table == nullptr || table->isEmpty())
                continue;

            const float level = juce::jlimit (0.0f, 2.0f,
                                              baseLevel[o] * (1.0f + mod.level[o]));
            if (level <= 0.0f)
                continue;

            const float wtPos = juce::jlimit (0.0f, 1.0f, baseWt[o] + mod.wt[o] * 0.5f);
            const float pan = juce::jlimit (-1.0f, 1.0f, basePan[o] + mod.pan[o]);
            const float lg = std::cos ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);
            const float rg = std::sin ((pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi);

            const float semitones = baseOct[o] * 12.0f + baseSemi[o] + mod.pitch[o] * 12.0f;
            const float spread = juce::jmax (0.0f, baseSpread[o] + mod.spread[o] * 50.0f);
            const int unison = juce::jlimit (1, WTSynth::maxUnison, (int) baseUnison[o]);
            const float norm = 1.0f / std::sqrt ((float) unison);

            for (auto& voice : voices)
            {
                if (! voice.active)
                    continue;

                float sum = 0.0f;

                for (int u = 0; u < unison; ++u)
                {
                    const float detune = (unison > 1)
                        ? spread * ((float) u / (float) (unison - 1) * 2.0f - 1.0f)
                        : 0.0f;

                    const float freq = 440.0f * std::pow (2.0f,
                        (voice.note - 69.0f + semitones + (baseFine[o] + detune) / 100.0f) / 12.0f);

                    voice.phase[o][u] += freq / sampleRate;
                    if (voice.phase[o][u] >= 1.0)
                        voice.phase[o][u] -= std::floor (voice.phase[o][u]);

                    sum += table->read ((float) voice.phase[o][u], wtPos);
                }

                const float out = sum * norm * level * voice.envValue;
                left  += out * lg;
                right += out * rg;
            }
        }

        //--- Filtro y salida -------------------------------------------------
        const float cutoff = juce::jlimit (20.0f, maxCutoff,
                                           baseCutoff * std::pow (2.0f, mod.cutoff * 4.0f));

        for (int ch = 0; ch < 2; ++ch)
        {
            filters[ch].setCutoffFrequency (cutoff);
            filters[ch].setResonance (reso);
        }

        left  = std::tanh (filters[0].processSample (0, left)  * 1.2f);
        right = std::tanh (filters[1].processSample (0, right) * 1.2f);

        buffer.setSample (0, sample, left  * master);
        buffer.setSample (1, sample, right * master);
    }
}

//==============================================================================
void WavetableSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    {
        juce::ScopedLock lock (wtLock);
        state.setProperty ("wav1", wavPaths[0], nullptr);
        state.setProperty ("wav2", wavPaths[1], nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void WavetableSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        const auto state = juce::ValueTree::fromXml (*xml);
        apvts.replaceState (state);

        for (int o = 0; o < WTSynth::numOscs; ++o)
        {
            const juce::String path = state.getProperty ("wav" + juce::String (o + 1)).toString();
            if (path.isNotEmpty())
                loadWavetable (o, juce::File (path));
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* WavetableSynthAudioProcessor::createEditor()
{
    return new WavetableSynthAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WavetableSynthAudioProcessor();
}
