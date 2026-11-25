#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

#include <array>
#include <memory>

class WavetableSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Button::Listener
{
public:
    explicit WavetableSynthAudioProcessorEditor (WavetableSynthAudioProcessor&);
    ~WavetableSynthAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct ParamSlider
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;

        explicit ParamSlider (const juce::String& text)
            : label ({}, text)
        {
            label.setJustificationType (juce::Justification::centredLeft);
            label.setFont (juce::FontOptions (12.0f));
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 18);
        }
    };

    struct ParamCombo
    {
        juce::ComboBox combo;
        std::unique_ptr<juce::ComboBoxParameterAttachment> attachment;

        ParamCombo() = default;
    };

    struct ModRow
    {
        std::unique_ptr<ParamCombo> target;
        std::unique_ptr<ParamSlider> amount;
    };

    void buttonClicked (juce::Button*) override;
    void loadForOsc (int osc);
    void refreshWavLabels();

    std::unique_ptr<ParamSlider> makeSlider (const juce::String& paramId,
                                             const juce::String& text);
    std::unique_ptr<ParamCombo> makeCombo (const juce::String& paramId,
                                           const juce::String& items,
                                           int firstItemId);

    void placeSlider (ParamSlider& control, int x, int y, int w, int h);

    WavetableSynthAudioProcessor& processorRef;

    std::array<std::unique_ptr<juce::TextButton>, 2> loadButtons;
    std::array<std::unique_ptr<juce::Label>, 2> wavLabels;
    std::array<std::unique_ptr<juce::FileChooser>, 2> fileChoosers;

    // [oscilador][parametro]: 0 WT, 1 Oct, 2 Semi, 3 Fine,
    //                         4 Unison, 5 Spread, 6 Level, 7 Pan
    std::array<std::array<std::unique_ptr<ParamSlider>, 8>, 2> oscSliders;

    std::array<std::unique_ptr<ParamSlider>, 4> envSliders;  // A, D, S, R
    std::array<std::unique_ptr<ParamSlider>, 2> filterSliders;
    std::unique_ptr<ParamSlider> masterSlider;
    std::unique_ptr<ParamSlider> lfoRateSlider;
    std::unique_ptr<ParamCombo> lfoShapeCombo;
    std::array<ModRow, WTSynth::numSlots> modRows;

    juce::Label hintLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavetableSynthAudioProcessorEditor)
};
