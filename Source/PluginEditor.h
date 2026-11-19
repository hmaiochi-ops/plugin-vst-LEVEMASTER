#pragma once
#include "PluginProcessor.h"

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
    void setActiveTab (int tabIndex);

    void drawMasterDashboard (juce::Graphics&, juce::Rectangle<int> area);
    void drawEqCurveGraph (juce::Graphics&, juce::Rectangle<int> area);
    void drawCompTransferGraph (juce::Graphics&, juce::Rectangle<int> area);
    void drawFinalTouchGraph (juce::Graphics&, juce::Rectangle<int> area);

    enum TabId { TabMaster = 0, TabEq = 1, TabCompSat = 2, TabFinal = 3 };

    struct ControlRow
    {
        int tab = 0;
        bool isHeader = false;
        juce::String text;
        juce::String paramId;
        juce::String bypassId;

        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAtt;
    };

    LeveMasterProcessor& processor;
    juce::LookAndFeel_V4 laf;

    int activeTab = TabMaster;
    juce::TextButton tabButtons[4];
    juce::TextButton resetLufsBtn { "Reset LUFS" };
    juce::ToggleButton masterBypass { "Bypass geral" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> masterBypassAtt;

    std::vector<std::unique_ptr<ControlRow>> rows;
    juce::Rectangle<int> graphArea;

    // Valores suavizados para renderização limpa dos medidores
    float dispPeakL = -60.0f, dispPeakR = -60.0f;
    float dispCompGr = 0.0f, dispLimGr = 0.0f;
    float dispLufsMom = -70.0f, dispLufsShort = -70.0f, dispLufsInt = -70.0f;
    float dispPhase = 1.0f, dispInLevel = -60.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LeveMasterEditor)
};
