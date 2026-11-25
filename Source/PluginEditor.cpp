#include "PluginEditor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
    juce::Colour panelBackground (0xff24272e);
    juce::Colour panelOutline   (0xff3d434e);
    juce::Colour titleColour    (0xff9ad7ff);
}

//==============================================================================
WavetableSynthAudioProcessorEditor::WavetableSynthAudioProcessorEditor (WavetableSynthAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    setSize (980, 600);

    for (int o = 0; o < WTSynth::numOscs; ++o)
    {
        loadButtons[o] = std::make_unique<juce::TextButton> (o == 0 ? "Cargar WAV 1" : "Cargar WAV 2");
        loadButtons[o]->addListener (this);
        addAndMakeVisible (*loadButtons[o]);

        wavLabels[o] = std::make_unique<juce::Label>();
        wavLabels[o]->setFont (juce::FontOptions (12.0f));
        wavLabels[o]->setColour (juce::Label::textColourId, juce::Colours::silver);
        addAndMakeVisible (*wavLabels[o]);
    }

    const char* ids[WTSynth::numOscs] = { "o1", "o2" };
    const char* names[8] = { "WT Pos", "Octave", "Semitone", "Fine",
                             "Unison", "Spread", "Level", "Pan" };
    const char* paramSuffixes[8] = { "wt", "oct", "semi", "fine",
                                     "unison", "spread", "level", "pan" };

    for (int o = 0; o < WTSynth::numOscs; ++o)
        for (int i = 0; i < 8; ++i)
        {
            oscSliders[o][i] = makeSlider (juce::String (ids[o]) + paramSuffixes[i], names[i]);
            addAndMakeVisible (oscSliders[o][i]->label);
            addAndMakeVisible (oscSliders[o][i]->slider);
        }

    const char* envNames[4] = { "Attack", "Decay", "Sustain", "Release" };
    const char* envIds[4]   = { "attack", "decay", "sustain", "release" };
    for (int i = 0; i < 4; ++i)
    {
        envSliders[i] = makeSlider (envIds[i], envNames[i]);
        addAndMakeVisible (envSliders[i]->label);
        addAndMakeVisible (envSliders[i]->slider);
    }

    filterSliders[0] = makeSlider ("cutoff", "Cutoff");
    filterSliders[1] = makeSlider ("reso", "Resonance");
    masterSlider = makeSlider ("master", "Master");

    for (auto& s : { filterSliders[0].get(), filterSliders[1].get(), masterSlider.get() })
    {
        addAndMakeVisible (s->label);
        addAndMakeVisible (s->slider);
    }

    lfoShapeCombo = makeCombo ("lfoShape", "Sine\nTriangle\nSaw\nSquare", 1);
    addAndMakeVisible (lfoShapeCombo->combo);

    lfoRateSlider = makeSlider ("lfoRate", "Rate");
    addAndMakeVisible (lfoRateSlider->label);
    addAndMakeVisible (lfoRateSlider->slider);

    juce::String targetItems;
    for (int i = 0; i < WTSynth::NumTargets; ++i)
        targetItems += juce::String (WTSynth::targetNames[i])
                     + (i + 1 < WTSynth::NumTargets ? "\n" : "");

    for (int i = 0; i < WTSynth::numSlots; ++i)
    {
        modRows[i].target = makeCombo ("m" + juce::String (i + 1) + "target", targetItems, 1);
        modRows[i].amount = makeSlider ("m" + juce::String (i + 1) + "amount",
                                        "Cant. " + juce::String (i + 1));
        addAndMakeVisible (modRows[i].target->combo);
        addAndMakeVisible (modRows[i].amount->label);
        addAndMakeVisible (modRows[i].amount->slider);
    }

    hintLabel.setText (
        "WAV de un ciclo o bloques consecutivos de 2048 muestras = frames. "
        "Sin WAV cargado el oscilador silencia. "
        "Cantidad negativa = modulacion en fase inversa.",
        juce::dontSendNotification);
    hintLabel.setFont (juce::FontOptions (11.0f));
    hintLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8c95a3));
    addAndMakeVisible (hintLabel);

    refreshWavLabels();
}

WavetableSynthAudioProcessorEditor::~WavetableSynthAudioProcessorEditor() = default;

//==============================================================================
std::unique_ptr<WavetableSynthAudioProcessorEditor::ParamSlider>
WavetableSynthAudioProcessorEditor::makeSlider (const juce::String& paramId,
                                                const juce::String& text)
{
    auto control = std::make_unique<ParamSlider> (text);

    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (
            processorRef.apvts.getParameter (paramId)))
    {
        control->attachment = std::make_unique<juce::SliderParameterAttachment> (
            *parameter, control->slider, nullptr);
    }
    else
    {
        jassertfalse;
    }

    return control;
}

std::unique_ptr<WavetableSynthAudioProcessorEditor::ParamCombo>
WavetableSynthAudioProcessorEditor::makeCombo (const juce::String& paramId,
                                               const juce::String& items,
                                               int firstItemId)
{
    auto control = std::make_unique<ParamCombo>();
    control->combo.addItemList (items, firstItemId);

    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (
            processorRef.apvts.getParameter (paramId)))
    {
        control->attachment = std::make_unique<juce::ComboBoxParameterAttachment> (
            *parameter, control->combo, nullptr);
    }
    else
    {
        jassertfalse;
    }

    return control;
}

void WavetableSynthAudioProcessorEditor::placeSlider (ParamSlider& control,
                                                      int x, int y, int w, int h)
{
    control.label.setBounds (x, y, 76, h);
    control.slider.setBounds (x + 78, y, juce::jmax (20, w - 78), h);
}

//==============================================================================
void WavetableSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14161a));

    g.setColour (titleColour);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("DUAL WAVETABLE SYNTH", 10, 8, 500, 28, juce::Justification::left);

    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));

    auto panel = [&g] (juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (panelBackground);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (panelOutline);
        g.drawRoundedRectangle (r.toFloat(), 6.0f, 1.0f);
        g.setColour (titleColour);
        g.drawText (title, r.getX(), r.getY() - 18, r.getWidth(), 16,
                    juce::Justification::centredLeft);
    };

    panel ({ 8, 48, 474, 284 }, "OSCILADOR 1");
    panel ({ 490, 48, 482, 284 }, "OSCILADOR 2");
    panel ({ 8, 340, 474, 100 }, "ENVELOPE (AMP)");
    panel ({ 490, 340, 240, 100 }, "FILTRO LP");
    panel ({ 738, 340, 234, 100 }, "MASTER");
    panel ({ 8, 448, 964, 144 }, "LFO  +  MATRIZ DE MODULACION");
}

void WavetableSynthAudioProcessorEditor::resized()
{
    auto placeOsc = [this] (int o, juce::Rectangle<int> r)
    {
        loadButtons[o]->setBounds (r.getX() + 10, r.getY() + 8, 136, 26);
        wavLabels[o]->setBounds (r.getX() + 154, r.getY() + 8, r.getWidth() - 164, 26);

        const int cellW = (r.getWidth() - 28) / 2;

        for (int i = 0; i < 8; ++i)
        {
            const int col = i / 4;
            const int row = i % 4;
            placeSlider (*oscSliders[o][i],
                         r.getX() + 10 + col * (cellW + 8),
                         r.getY() + 44 + row * 57,
                         cellW, 52);
        }
    };

    placeOsc (0, { 8, 48, 474, 284 });
    placeOsc (1, { 490, 48, 482, 284 });

    const juce::Rectangle<int> envR (8, 340, 474, 100);
    for (int i = 0; i < 4; ++i)
        placeSlider (*envSliders[i], envR.getX() + 10, envR.getY() + 6 + i * 23,
                     envR.getWidth() - 20, 22);

    placeSlider (*filterSliders[0], 500, 354, 220, 32);
    placeSlider (*filterSliders[1], 500, 392, 220, 32);
    placeSlider (*masterSlider, 748, 388, 214, 34);

    const juce::Rectangle<int> lfoR (8, 448, 964, 144);
    lfoShapeCombo->combo.setBounds (lfoR.getX() + 10, lfoR.getY() + 8, 150, 26);
    placeSlider (*lfoRateSlider, lfoR.getX() + 170, lfoR.getY() + 8, 260, 26);

    for (int i = 0; i < WTSynth::numSlots; ++i)
    {
        const int col = i % 2;
        const int row = i / 2;
        const int x = lfoR.getX() + 10 + col * 480;
        const int y = lfoR.getY() + 42 + row * 32;

        modRows[i].target->combo.setBounds (x, y, 230, 24);
        placeSlider (*modRows[i].amount, x + 240, y, 225, 24);
    }

    hintLabel.setBounds (lfoR.getX() + 10, lfoR.getY() + 110, lfoR.getWidth() - 20, 28);
}

//==============================================================================
void WavetableSynthAudioProcessorEditor::refreshWavLabels()
{
    for (int o = 0; o < WTSynth::numOscs; ++o)
    {
        auto table = processorRef.getWavetable (o);

        if (table != nullptr && ! table->isEmpty())
            wavLabels[o]->setText (table->getFileName() + "  (" +
                                   juce::String (table->getNumFrames()) + " frames)",
                                   juce::dontSendNotification);
        else
            wavLabels[o]->setText ("Sin wavetable - cargar WAV", juce::dontSendNotification);
    }
}

void WavetableSynthAudioProcessorEditor::loadForOsc (int osc)
{
    fileChoosers[osc] = std::make_unique<juce::FileChooser> (
        "Cargar wavetable WAV", juce::File(), "*.wav;*.WAV");

    juce::Component::SafePointer<WavetableSynthAudioProcessorEditor> safe (this);

    fileChoosers[osc]->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe, osc] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();

            if (file.existsAsFile() && safe.getComponent() != nullptr)
            {
                safe->processorRef.loadWavetable (osc, file);
                safe->refreshWavLabels();
            }
        });
}

void WavetableSynthAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == loadButtons[0].get())
        loadForOsc (0);
    else if (button == loadButtons[1].get())
        loadForOsc (1);
}
