#include "PluginEditor.h"

namespace
{
    const juce::Colour bg      { 0xff17191d };
    const juce::Colour panel   { 0xff22252b };
    const juce::Colour accent  { 0xffd9a441 };
    const juce::Colour textCol { 0xffd8dadf };
    const juce::Colour dimCol  { 0xff8a8f99 };

    constexpr int colWidth = 360, margin = 14, topY = 44, rowH = 26, headH = 32;
}

LeveMasterEditor::LeveMasterEditor (LeveMasterProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    laf.setColour (juce::Slider::trackColourId, accent);
    laf.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff3a3e46));
    laf.setColour (juce::Slider::thumbColourId, textCol);
    laf.setColour (juce::Slider::textBoxTextColourId, textCol);
    laf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    laf.setColour (juce::Slider::textBoxBackgroundColourId, panel);
    laf.setColour (juce::Label::textColourId, textCol);
    laf.setColour (juce::ToggleButton::textColourId, dimCol);
    laf.setColour (juce::ToggleButton::tickColourId, accent);
    laf.setColour (juce::ToggleButton::tickDisabledColourId, dimCol);
    setLookAndFeel (&laf);

    using I = Item;
    const std::vector<I> layout
    {
        { I::Header, "ENTRADA", "", "" },
        { I::Slider, "Entrada", ids::inGain, "" },

        { I::Header, "EQ", "", ids::bypEq },
        { I::Slider, "Grave 100 Hz", ids::bass, "" },
        { I::Slider, "Médio 2,5 kHz", ids::mid, "" },
        { I::Slider, "Agudo 10 kHz", ids::treble, "" },

        { I::Header, "COMPRESSOR", "", ids::bypComp },
        { I::Slider, "Threshold", ids::thresh, "" },
        { I::Slider, "Ratio", ids::ratio, "" },
        { I::Slider, "Ataque", ids::attack, "" },
        { I::Slider, "Soltura", ids::release, "" },
        { I::Slider, "Mix paralelo", ids::mix, "" },

        { I::NextColumn, "", "", "" },

        { I::Header, "SATURAÇÃO", "", ids::bypSat },
        { I::Slider, "Drive", ids::drive, "" },

        { I::Header, "ESTÉREO", "", ids::bypWidth },
        { I::Slider, "Largura", ids::width, "" },

        { I::Header, "LIMITER", "", ids::bypLim },
        { I::Slider, "Push", ids::push, "" },
        { I::Slider, "Teto", ids::ceiling, "" },
        { I::Slider, "Soltura", ids::limRel, "" },

        { I::Header, "SAÍDA", "", "" },
        { I::Slider, "Saída", ids::outGain, "" },
    };

    for (auto& item : layout)
    {
        auto row = std::make_unique<Row>();
        row->item = item;

        if (item.kind == I::Header)
        {
            row->label = std::make_unique<juce::Label>();
            row->label->setText (item.text, juce::dontSendNotification);
            row->label->setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
            row->label->setColour (juce::Label::textColourId, accent);
            addAndMakeVisible (*row->label);

            if (item.bypassId.isNotEmpty())
            {
                row->toggle = std::make_unique<juce::ToggleButton> ("bypass");
                addAndMakeVisible (*row->toggle);
                row->buttonAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                    processor.apvts, item.bypassId, *row->toggle);
            }
        }
        else if (item.kind == I::Slider)
        {
            row->label = std::make_unique<juce::Label>();
            row->label->setText (item.text, juce::dontSendNotification);
            row->label->setFont (juce::Font (juce::FontOptions (13.0f)));
            addAndMakeVisible (*row->label);

            row->slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            row->slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 74, 20);
            addAndMakeVisible (*row->slider);
            row->sliderAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                processor.apvts, item.id, *row->slider);

            if (auto* param = processor.apvts.getParameter (item.id))
                row->slider->setDoubleClickReturnValue (true,
                    param->convertFrom0to1 (param->getDefaultValue()));
        }
        rows.push_back (std::move (row));
    }

    masterBypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, ids::bypass, masterBypass);
    addAndMakeVisible (masterBypass);

    meterLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    meterLabel.setColour (juce::Label::textColourId, dimCol);
    meterLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (meterLabel);

    setSize (margin * 3 + colWidth * 2, 440);
    startTimerHz (10);
}

LeveMasterEditor::~LeveMasterEditor()
{
    setLookAndFeel (nullptr);
}

void LeveMasterEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setColour (textCol);
    g.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    g.drawText ("LeveMaster", margin, 8, 200, 28, juce::Justification::centredLeft);

    g.setColour (panel);
    g.fillRect (margin * 2 + colWidth - 1, topY, 1, getHeight() - topY - 48);
}

void LeveMasterEditor::resized()
{
    masterBypass.setBounds (getWidth() - margin - 130, 10, 130, 24);

    int x = margin, y = topY;
    for (auto& row : rows)
    {
        switch (row->item.kind)
        {
            case Item::NextColumn:
                x = margin * 2 + colWidth;
                y = topY;
                break;

            case Item::Header:
                y += 6;
                row->label->setBounds (x, y, 200, headH - 6);
                if (row->toggle != nullptr)
                    row->toggle->setBounds (x + colWidth - 90, y, 90, headH - 6);
                y += headH - 6;
                break;

            case Item::Slider:
                row->label->setBounds (x, y, 120, rowH);
                row->slider->setBounds (x + 120, y, colWidth - 120, rowH);
                y += rowH;
                break;
        }
    }

    meterLabel.setBounds (margin, getHeight() - 38, getWidth() - margin * 2, 24);
}

void LeveMasterEditor::timerCallback()
{
    const float comp = processor.meterCompGr.load(), lim = processor.meterLimGr.load(),
                peak = processor.meterPeak.load();
    meterLabel.setText ("Comp  -" + juce::String (comp, 1) + " dB      Limiter  -" + juce::String (lim, 1)
                        + " dB      Pico saída  " + (peak <= -100.0f ? juce::String ("-inf") : juce::String (peak, 1))
                        + " dBFS",
                        juce::dontSendNotification);
}
