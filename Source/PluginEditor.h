#pragma once
#include "PluginProcessor.h"

// Interface minimalista: só sliders, botões de bypass e uma linha de texto com medidores.
// Sem bitmaps, sem animação, sem OpenGL.
class LeveMasterEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit LeveMasterEditor (LeveMasterProcessor&);
    ~LeveMasterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Item
    {
        enum Kind { Header, Slider, NextColumn } kind;
        juce::String text, id, bypassId;
    };

    struct Row
    {
        Item item;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAtt;
    };

    LeveMasterProcessor& processor;
    juce::LookAndFeel_V4 laf;
    std::vector<std::unique_ptr<Row>> rows;
    juce::ToggleButton masterBypass { "Bypass geral" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> masterBypassAtt;
    juce::Label meterLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LeveMasterEditor)
};
