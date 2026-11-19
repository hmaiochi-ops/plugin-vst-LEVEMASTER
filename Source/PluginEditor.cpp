#include "PluginEditor.h"

namespace
{
    const juce::Colour bg       { 0xff14161a };
    const juce::Colour panel    { 0xff1d2026 };
    const juce::Colour border   { 0xff2c313a };
    const juce::Colour accent   { 0xffd9a441 };
    const juce::Colour cyanCol  { 0xff49b6d6 };
    const juce::Colour greenCol { 0xff4cc38a };
    const juce::Colour redCol   { 0xffe05a5a };
    const juce::Colour textCol  { 0xffe2e4e9 };
    const juce::Colour dimCol   { 0xff8a8f99 };

    constexpr int winW = 840;
    constexpr int winH = 510;
    constexpr int margin = 14;
}

LeveMasterEditor::LeveMasterEditor (LeveMasterProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    laf.setColour (juce::Slider::trackColourId, accent);
    laf.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff323741));
    laf.setColour (juce::Slider::thumbColourId, textCol);
    laf.setColour (juce::Slider::textBoxTextColourId, textCol);
    laf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    laf.setColour (juce::Slider::textBoxBackgroundColourId, panel);
    laf.setColour (juce::Label::textColourId, textCol);
    laf.setColour (juce::ToggleButton::textColourId, dimCol);
    laf.setColour (juce::ToggleButton::tickColourId, accent);
    laf.setColour (juce::ToggleButton::tickDisabledColourId, dimCol);
    setLookAndFeel (&laf);

    const char* tabNames[4] = { "MASTER", "EQ (20-20k)", "COMP & SAT", "FINAL TOUCH" };
    for (int i = 0; i < 4; ++i)
    {
        tabButtons[i].setButtonText (juce::CharPointer_UTF8 (tabNames[i]));
        tabButtons[i].setClickingTogglesState (false);
        tabButtons[i].onClick = [this, i] { setActiveTab (i); };
        addAndMakeVisible (tabButtons[i]);
    }

    resetLufsBtn.setColour (juce::TextButton::buttonColourId, panel);
    resetLufsBtn.setColour (juce::TextButton::textColourOffId, dimCol);
    resetLufsBtn.onClick = [this] { processor.resetLufs(); };
    addAndMakeVisible (resetLufsBtn);

    masterBypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, ids::bypass, masterBypass);
    addAndMakeVisible (masterBypass);

    auto addHeader = [this] (int tab, const char* title, const char* bypId)
    {
        auto r = std::make_unique<ControlRow>();
        r->tab = tab;
        r->isHeader = true;
        r->text = juce::CharPointer_UTF8 (title);
        r->bypassId = bypId;

        r->label = std::make_unique<juce::Label>();
        r->label->setText (r->text, juce::dontSendNotification);
        r->label->setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        r->label->setColour (juce::Label::textColourId, accent);
        addChildComponent (*r->label);

        if (r->bypassId.isNotEmpty())
        {
            r->toggle = std::make_unique<juce::ToggleButton> ("bypass");
            addChildComponent (*r->toggle);
            r->buttonAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                processor.apvts, r->bypassId, *r->toggle);
        }
        rows.push_back (std::move (r));
    };

    auto addSlider = [this] (int tab, const char* labelTxt, const char* paramId)
    {
        auto r = std::make_unique<ControlRow>();
        r->tab = tab;
        r->isHeader = false;
        r->text = juce::CharPointer_UTF8 (labelTxt);
        r->paramId = paramId;

        r->label = std::make_unique<juce::Label>();
        r->label->setText (r->text, juce::dontSendNotification);
        r->label->setFont (juce::Font (juce::FontOptions (13.0f)));
        addChildComponent (*r->label);

        r->slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        r->slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 20);
        addChildComponent (*r->slider);
        r->sliderAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, r->paramId, *r->slider);

        if (auto* param = processor.apvts.getParameter (r->paramId))
            r->slider->setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

        rows.push_back (std::move (r));
    };

    // Aba 0: MASTER (visão rápida e ganho estrutural)
    addHeader (TabMaster, "GANHO ESTRUTURAL", "");
    addSlider (TabMaster, "Entrada", ids::inGain);
    addSlider (TabMaster, "Push Limiter", ids::push);
    addSlider (TabMaster, "Teto (Ceiling)", ids::ceiling);
    addSlider (TabMaster, "Saída Final", ids::outGain);
    addHeader (TabMaster, "EQUILÍBRIO RÁPIDO", ids::bypEq);
    addSlider (TabMaster, "Grave 100 Hz", ids::bass);
    addSlider (TabMaster, "Agudo 10 kHz", ids::treble);

    // Aba 1: EQ (20 Hz - 20 kHz)
    addHeader (TabEq, "EQUALIZADOR MASTERING", ids::bypEq);
    addSlider (TabEq, "Grave (100 Hz Shelf)", ids::bass);
    addSlider (TabEq, "Médio (2,5 kHz Sino)", ids::mid);
    addSlider (TabEq, "Agudo (10 kHz Shelf)", ids::treble);

    // Aba 2: COMP & SAT
    addHeader (TabCompSat, "COMPRESSOR DE BUS", ids::bypComp);
    addSlider (TabCompSat, "Threshold", ids::thresh);
    addSlider (TabCompSat, "Ratio", ids::ratio);
    addSlider (TabCompSat, "Ataque", ids::attack);
    addSlider (TabCompSat, "Soltura", ids::release);
    addSlider (TabCompSat, "Mix paralelo", ids::mix);
    addHeader (TabCompSat, "SATURAÇÃO HARMÔNICA", ids::bypSat);
    addSlider (TabCompSat, "Drive (2x OS)", ids::drive);

    // Aba 3: FINAL TOUCH & LIMITER
    addHeader (TabFinal, "IMAGEM ESTÉREO", ids::bypWidth);
    addSlider (TabFinal, "Largura M/S", ids::width);
    addHeader (TabFinal, "LIMITER BRICKWALL", ids::bypLim);
    addSlider (TabFinal, "Push", ids::push);
    addSlider (TabFinal, "Teto", ids::ceiling);
    addSlider (TabFinal, "Soltura", ids::limRel);
    addHeader (TabFinal, "ESTÁGIO DE SAÍDA", "");
    addSlider (TabFinal, "Saída", ids::outGain);

    setSize (winW, winH);
    setActiveTab (TabMaster);
    startTimerHz (24);
}

LeveMasterEditor::~LeveMasterEditor()
{
    setLookAndFeel (nullptr);
}

void LeveMasterEditor::setActiveTab (int tabIndex)
{
    activeTab = tabIndex;
    for (int i = 0; i < 4; ++i)
    {
        const bool isSel = (i == activeTab);
        tabButtons[i].setColour (juce::TextButton::buttonColourId, isSel ? accent : panel);
        tabButtons[i].setColour (juce::TextButton::textColourOffId, isSel ? bg : textCol);
    }

    for (auto& r : rows)
    {
        const bool show = (r->tab == activeTab);
        if (r->label)  r->label->setVisible (show);
        if (r->slider) r->slider->setVisible (show);
        if (r->toggle) r->toggle->setVisible (show);
    }

    resized();
    repaint();
}

void LeveMasterEditor::resized()
{
    const int topBarH = 44;
    masterBypass.setBounds (getWidth() - margin - 120, 10, 120, 24);
    resetLufsBtn.setBounds (getWidth() - margin - 216, 10, 88, 24);

    int tabX = 155;
    for (int i = 0; i < 4; ++i)
    {
        tabButtons[i].setBounds (tabX, 10, 102, 24);
        tabX += 108;
    }

    const int leftColW = 365;
    const int contentTop = topBarH + 10;
    const int bottomBarH = 46;

    int y = contentTop;
    for (auto& r : rows)
    {
        if (r->tab != activeTab) continue;

        if (r->isHeader)
        {
            y += 8;
            r->label->setBounds (margin, y, 220, 24);
            if (r->toggle)
                r->toggle->setBounds (margin + leftColW - 82, y, 82, 24);
            y += 26;
        }
        else
        {
            r->label->setBounds (margin, y, 138, 26);
            r->slider->setBounds (margin + 138, y, leftColW - 138, 26);
            y += 28;
        }
    }

    const int graphX = margin * 2 + leftColW;
    const int graphW = getWidth() - graphX - margin;
    const int graphH = getHeight() - contentTop - bottomBarH - 8;
    graphArea = juce::Rectangle<int> (graphX, contentTop, graphW, graphH);
}

void LeveMasterEditor::timerCallback()
{
    dispCompGr    = processor.meterCompGr.load();
    dispLimGr     = processor.meterLimGr.load();
    dispPeakL     = processor.meterPeakL.load();
    dispPeakR     = processor.meterPeakR.load();
    dispLufsMom   = processor.meterLufsMom.load();
    dispLufsShort = processor.meterLufsShort.load();
    dispLufsInt   = processor.meterLufsInt.load();
    dispPhase     = processor.meterPhase.load();
    dispInLevel   = processor.meterInLevel.load();
    repaint();
}

void LeveMasterEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    // Barra superior
    g.setColour (panel);
    g.fillRect (0, 0, getWidth(), 44);
    g.setColour (border);
    g.drawLine (0.0f, 44.0f, (float) getWidth(), 44.0f);

    g.setColour (textCol);
    g.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));
    g.drawText ("LeveMaster", margin, 8, 135, 28, juce::Justification::centredLeft);

    // Caixa do visualizador gráfico à direita
    g.setColour (panel);
    g.fillRoundedRectangle (graphArea.toFloat(), 6.0f);
    g.setColour (border);
    g.drawRoundedRectangle (graphArea.toFloat(), 6.0f, 1.0f);

    auto inner = graphArea.reduced (12);
    if (activeTab == TabMaster)       drawMasterDashboard (g, inner);
    else if (activeTab == TabEq)      drawEqCurveGraph (g, inner);
    else if (activeTab == TabCompSat) drawCompTransferGraph (g, inner);
    else                              drawFinalTouchGraph (g, inner);

    // Barra inferior fixa com telemetria LUFS + GR + Peak
    const int barY = getHeight() - 40;
    g.setColour (panel);
    g.fillRect (0, barY, getWidth(), 40);
    g.setColour (border);
    g.drawLine (0.0f, (float) barY, (float) getWidth(), (float) barY);

    auto fmtDb = [] (float v, float floorVal) -> juce::String
    {
        return v <= floorVal ? "-inf" : juce::String (v, 1);
    };

    const float maxPeak = std::max (dispPeakL, dispPeakR);
    juce::String status =
        "LUFS  M: " + fmtDb (dispLufsMom, -69.5f) +
        "   S: " + fmtDb (dispLufsShort, -69.5f) +
        "   I: " + fmtDb (dispLufsInt, -69.5f) +
        "      |      Comp GR: -" + juce::String (dispCompGr, 1) + " dB" +
        "      Lim GR: -" + juce::String (dispLimGr, 1) + " dB" +
        "      Pico: " + fmtDb (maxPeak, -100.0f) + " dBFS";

    g.setColour (textCol);
    g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
    g.drawText (juce::CharPointer_UTF8 (status.toRawUTF8()), margin, barY + 6, getWidth() - margin * 2, 28,
                juce::Justification::centredLeft);
}

//==============================================================================
void LeveMasterEditor::drawMasterDashboard (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (accent);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("TELEMETRIA DE MASTERIZAÇÃO (ITU-R BS.1770-4)", area.removeFromTop (22),
                juce::Justification::centredLeft);

    area.removeFromTop (8);

    // 3 cartões numéricos LUFS no topo
    auto cardsRow = area.removeFromTop (68);
    const int cardW = (cardsRow.getWidth() - 16) / 3;
    struct LufsCard { const char* title; float val; juce::Colour col; };
    const LufsCard cards[3] = {
        { "MOMENTARY (400ms)", dispLufsMom, cyanCol },
        { "SHORT-TERM (3s)",   dispLufsShort, accent },
        { "INTEGRATED",        dispLufsInt, greenCol }
    };

    for (int i = 0; i < 3; ++i)
    {
        auto cBox = cardsRow.removeFromLeft (cardW);
        if (i < 2) cardsRow.removeFromLeft (8);

        g.setColour (bg);
        g.fillRoundedRectangle (cBox.toFloat(), 4.0f);
        g.setColour (border);
        g.drawRoundedRectangle (cBox.toFloat(), 4.0f, 1.0f);

        g.setColour (dimCol);
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText (cards[i].title, cBox.reduced (8, 6).removeFromTop (16), juce::Justification::centred);

        g.setColour (cards[i].col);
        g.setFont (juce::Font (juce::FontOptions (21.0f, juce::Font::bold)));
        juce::String txt = cards[i].val <= -69.5f ? "-inf" : juce::String (cards[i].val, 1) + " LUFS";
        g.drawText (txt, cBox.reduced (6, 22), juce::Justification::centred);
    }

    area.removeFromTop (14);

    // Barras horizontais de nível: Saída L, Saída R, Short-Term LUFS e Redução GR
    auto drawBar = [&] (const char* label, float val, float minV, float maxV, juce::Colour col, const juce::String& readOut)
    {
        auto row = area.removeFromTop (34);
        auto topText = row.removeFromTop (16);
        g.setColour (textCol);
        g.setFont (juce::Font (juce::FontOptions (11.5f)));
        g.drawText (juce::CharPointer_UTF8 (label), topText, juce::Justification::centredLeft);
        g.setColour (dimCol);
        g.drawText (readOut, topText, juce::Justification::centredRight);

        auto track = row.reduced (0, 2).toFloat();
        g.setColour (bg);
        g.fillRoundedRectangle (track, 3.0f);

        const float norm = juce::jlimit (0.0f, 1.0f, (val - minV) / (maxV - minV));
        if (norm > 0.001f)
        {
            g.setColour (col);
            g.fillRoundedRectangle (track.withWidth (track.getWidth() * norm), 3.0f);
        }
        area.removeFromTop (6);
    };

    drawBar ("Saída Canal L (dBFS)", dispPeakL, -60.0f, 0.0f,
             dispPeakL > -0.3f ? redCol : greenCol,
             dispPeakL <= -100.0f ? "-inf dB" : juce::String (dispPeakL, 1) + " dB");

    drawBar ("Saída Canal R (dBFS)", dispPeakR, -60.0f, 0.0f,
             dispPeakR > -0.3f ? redCol : greenCol,
             dispPeakR <= -100.0f ? "-inf dB" : juce::String (dispPeakR, 1) + " dB");

    drawBar ("Loudness Short-Term (-36 a 0 LUFS)", dispLufsShort, -36.0f, 0.0f,
             dispLufsShort > -9.0f ? redCol : accent,
             dispLufsShort <= -69.5f ? "-inf" : juce::String (dispLufsShort, 1) + " LUFS");

    drawBar ("Redução Total (Comp + Limiter)", dispCompGr + dispLimGr, 0.0f, 18.0f,
             cyanCol, "-" + juce::String (dispCompGr + dispLimGr, 1) + " dB");

    area.removeFromTop (6);

    // Barra de Correlação de Fase (-1 a +1)
    auto phaseRow = area.removeFromTop (36);
    g.setColour (textCol);
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawText (juce::CharPointer_UTF8 ("Correlação de Fase (-1 / 0 / +1)"),
                phaseRow.removeFromTop (16), juce::Justification::centredLeft);
    g.setColour (dispPhase < 0.0f ? redCol : greenCol);
    g.drawText (juce::String (dispPhase, 2), phaseRow.withY (phaseRow.getY() - 16).withHeight (16),
                juce::Justification::centredRight);

    auto pTrack = phaseRow.reduced (0, 3).toFloat();
    g.setColour (bg);
    g.fillRoundedRectangle (pTrack, 3.0f);
    const float centerX = pTrack.getCentreX();
    g.setColour (border);
    g.drawLine (centerX, pTrack.getY(), centerX, pTrack.getBottom(), 1.5f);

    const float pNorm = juce::jlimit (-1.0f, 1.0f, dispPhase);
    const float markerX = centerX + pNorm * (pTrack.getWidth() * 0.5f);
    g.setColour (pNorm < 0.0f ? redCol : greenCol);
    g.fillRoundedRectangle (juce::Rectangle<float> (std::min (centerX, markerX), pTrack.getY(),
                                                    std::max (3.0f, std::abs (markerX - centerX)), pTrack.getHeight()), 2.0f);
}

//==============================================================================
void LeveMasterEditor::drawEqCurveGraph (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (accent);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("CURVA DE EQUALIZAÇÃO (20 Hz – 20 kHz)", area.removeFromTop (22),
                juce::Justification::centredLeft);

    area.removeFromTop (6);
    auto plot = area.reduced (28, 18).toFloat();

    g.setColour (bg);
    g.fillRoundedRectangle (plot, 4.0f);
    g.setColour (border);
    g.drawRoundedRectangle (plot, 4.0f, 1.0f);

    // Grade horizontal de dB (-9 a +9 dB)
    const float dbTicks[5] = { 9.0f, 4.5f, 0.0f, -4.5f, -9.0f };
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    for (float db : dbTicks)
    {
        const float y = juce::jmap (db, -9.0f, 9.0f, plot.getBottom(), plot.getY());
        g.setColour (db == 0.0f ? dimCol.withAlpha (0.6f) : border);
        g.drawLine (plot.getX(), y, plot.getRight(), y, db == 0.0f ? 1.2f : 0.8f);
        g.setColour (dimCol);
        g.drawText (juce::String ((int) db) + "dB", (int) plot.getX() - 30, (int) y - 7, 26, 14,
                    juce::Justification::centredRight);
    }

    // Grade logarítmica de frequências (20 Hz a 20 kHz)
    const double freqs[9] = { 20.0, 50.0, 100.0, 250.0, 500.0, 1000.0, 2500.0, 10000.0, 20000.0 };
    const char* fLabels[9] = { "20", "50", "100", "250", "500", "1k", "2.5k", "10k", "20k" };
    const double logMin = std::log10 (20.0);
    const double logMax = std::log10 (20000.0);

    for (int i = 0; i < 9; ++i)
    {
        const float normX = (float) ((std::log10 (freqs[i]) - logMin) / (logMax - logMin));
        const float x = plot.getX() + normX * plot.getWidth();
        g.setColour (border);
        g.drawLine (x, plot.getY(), x, plot.getBottom(), 0.8f);
        g.setColour (dimCol);
        g.drawText (fLabels[i], (int) x - 16, (int) plot.getBottom() + 2, 32, 14, juce::Justification::centred);
    }

    // Curva de resposta combinada
    const float bassDb   = processor.apvts.getRawParameterValue (ids::bass)->load();
    const float midDb    = processor.apvts.getRawParameterValue (ids::mid)->load();
    const float trebleDb = processor.apvts.getRawParameterValue (ids::treble)->load();
    const bool bypEq     = processor.apvts.getRawParameterValue (ids::bypEq)->load() > 0.5f;
    const double sr      = processor.getCurrentSampleRate();

    juce::Path curve;
    juce::Path fillPath;
    const float zeroY = juce::jmap (0.0f, -9.0f, 9.0f, plot.getBottom(), plot.getY());
    const int steps = (int) plot.getWidth();

    for (int px = 0; px <= steps; ++px)
    {
        const double norm = (double) px / (double) std::max (1, steps);
        const double f = std::pow (10.0, logMin + norm * (logMax - logMin));
        const float magDb = juce::jlimit (-9.0f, 9.0f,
            leve::Eq3::computeCombinedDb (sr, f, bassDb, midDb, trebleDb, bypEq));

        const float x = plot.getX() + (float) px;
        const float y = juce::jmap (magDb, -9.0f, 9.0f, plot.getBottom(), plot.getY());

        if (px == 0)
        {
            curve.startNewSubPath (x, y);
            fillPath.startNewSubPath (x, zeroY);
            fillPath.lineTo (x, y);
        }
        else
        {
            curve.lineTo (x, y);
            fillPath.lineTo (x, y);
        }
    }
    fillPath.lineTo (plot.getRight(), zeroY);
    fillPath.closeSubPath();

    g.setColour ((bypEq ? dimCol : accent).withAlpha (0.18f));
    g.fillPath (fillPath);

    g.setColour (bypEq ? dimCol : accent);
    g.strokePath (curve, juce::PathStrokeType (2.2f));
}

//==============================================================================
void LeveMasterEditor::drawCompTransferGraph (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (accent);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("CURVA DE TRANSFERÊNCIA DO COMPRESSOR", area.removeFromTop (22),
                juce::Justification::centredLeft);

    area.removeFromTop (6);
    auto rightMeter = area.removeFromRight (52);
    area.removeFromRight (12);
    auto plot = area.reduced (24, 18).toFloat();

    g.setColour (bg);
    g.fillRoundedRectangle (plot, 4.0f);
    g.setColour (border);
    g.drawRoundedRectangle (plot, 4.0f, 1.0f);

    // Grade -40 dB a 0 dB
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    for (int db = -40; db <= 0; db += 10)
    {
        const float norm = (float) (db + 40) / 40.0f;
        const float x = plot.getX() + norm * plot.getWidth();
        const float y = plot.getBottom() - norm * plot.getHeight();
        g.setColour (border);
        g.drawLine (x, plot.getY(), x, plot.getBottom(), 0.8f);
        g.drawLine (plot.getX(), y, plot.getRight(), y, 0.8f);
        g.setColour (dimCol);
        g.drawText (juce::String (db), (int) x - 12, (int) plot.getBottom() + 2, 24, 12, juce::Justification::centred);
        g.drawText (juce::String (db), (int) plot.getX() - 26, (int) y - 6, 22, 12, juce::Justification::centredRight);
    }

    // Linha diagonal 1:1 de referência
    g.setColour (dimCol.withAlpha (0.4f));
    g.drawLine (plot.getX(), plot.getBottom(), plot.getRight(), plot.getY(), 1.0f);

    const float thr   = processor.apvts.getRawParameterValue (ids::thresh)->load();
    const float ratio = processor.apvts.getRawParameterValue (ids::ratio)->load();
    const float mix01 = processor.apvts.getRawParameterValue (ids::mix)->load() * 0.01f;
    const bool byp    = processor.apvts.getRawParameterValue (ids::bypComp)->load() > 0.5f;

    juce::Path transfer;
    for (int i = 0; i <= 80; ++i)
    {
        const float inDb = -40.0f + (float) i * 0.5f;
        const float outDb = juce::jlimit (-40.0f, 0.0f,
            leve::LinkedCompressor::computeTransferDb (inDb, thr, ratio, mix01, byp));

        const float x = juce::jmap (inDb, -40.0f, 0.0f, plot.getX(), plot.getRight());
        const float y = juce::jmap (outDb, -40.0f, 0.0f, plot.getBottom(), plot.getY());
        if (i == 0) transfer.startNewSubPath (x, y);
        else        transfer.lineTo (x, y);
    }

    g.setColour (byp ? dimCol : cyanCol);
    g.strokePath (transfer, juce::PathStrokeType (2.3f));

    // Ponto operacional atual no gráfico
    if (! byp && dispInLevel > -40.0f)
    {
        const float curIn = juce::jlimit (-40.0f, 0.0f, dispInLevel);
        const float curOut = juce::jlimit (-40.0f, 0.0f,
            leve::LinkedCompressor::computeTransferDb (curIn, thr, ratio, mix01, false));
        const float px = juce::jmap (curIn, -40.0f, 0.0f, plot.getX(), plot.getRight());
        const float py = juce::jmap (curOut, -40.0f, 0.0f, plot.getBottom(), plot.getY());
        g.setColour (accent);
        g.fillEllipse (px - 4.5f, py - 4.5f, 9.0f, 9.0f);
    }

    // Medidor vertical de Gain Reduction (GR)
    g.setColour (dimCol);
    g.drawText ("GR", rightMeter.removeFromTop (16), juce::Justification::centred);
    auto grTrack = rightMeter.reduced (12, 4).toFloat();
    g.setColour (bg);
    g.fillRoundedRectangle (grTrack, 3.0f);
    const float grNorm = juce::jlimit (0.0f, 1.0f, dispCompGr / 15.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (grTrack.withHeight (grTrack.getHeight() * grNorm), 3.0f);
}

//==============================================================================
void LeveMasterEditor::drawFinalTouchGraph (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (accent);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("ESTÁGIO FINAL: LIMITER & IMAGEM ESTÉREO", area.removeFromTop (22),
                juce::Justification::centredLeft);

    area.removeFromTop (8);

    // Visualizador de largura estéreo (Mid / Side)
    auto stereoBox = area.removeFromTop (125).toFloat();
    g.setColour (bg);
    g.fillRoundedRectangle (stereoBox, 4.0f);
    g.setColour (border);
    g.drawRoundedRectangle (stereoBox, 4.0f, 1.0f);

    const float width = processor.apvts.getRawParameterValue (ids::width)->load() * 0.01f;
    const bool bypW   = processor.apvts.getRawParameterValue (ids::bypWidth)->load() > 0.5f;
    const float effW  = bypW ? 1.0f : width;

    const float cx = stereoBox.getCentreX();
    const float cy = stereoBox.getBottom() - 16.0f;
    const float radius = std::min (stereoBox.getWidth() * 0.42f, stereoBox.getHeight() - 28.0f);

    // Arco de referência 100% estéreo (90 graus)
    g.setColour (dimCol.withAlpha (0.4f));
    g.drawLine (cx, cy, cx - radius * 0.707f, cy - radius * 0.707f, 1.0f);
    g.drawLine (cx, cy, cx + radius * 0.707f, cy - radius * 0.707f, 1.0f);

    // Leque atual de abertura estéreo
    const float angle = juce::jlimit (0.0f, 1.35f, effW * 0.7854f);
    const float lx = cx - radius * std::sin (angle);
    const float ly = cy - radius * std::cos (angle);
    const float rx = cx + radius * std::sin (angle);
    const float ry = cy - radius * std::cos (angle);

    juce::Path fan;
    fan.startNewSubPath (cx, cy);
    fan.lineTo (lx, ly);
    fan.lineTo (rx, ry);
    fan.closeSubPath();

    g.setColour ((bypW ? dimCol : greenCol).withAlpha (0.22f));
    g.fillPath (fan);
    g.setColour (bypW ? dimCol : greenCol);
    g.drawLine (cx, cy, lx, ly, 2.0f);
    g.drawLine (cx, cy, rx, ry, 2.0f);

    g.setColour (textCol);
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawText ("Abertura Estéreo: " + juce::String ((int) std::round (effW * 100.0f)) + "%   |   Fase: " + juce::String (dispPhase, 2),
                stereoBox.reduced (8, 4).removeFromTop (18), juce::Justification::centred);

    area.removeFromTop (12);

    // Medidor de Redução do Limiter (Ceiling vs Gain Reduction)
    const float ceilDb = processor.apvts.getRawParameterValue (ids::ceiling)->load();
    g.setColour (textCol);
    g.drawText ("Redução do Limiter Brickwall: -" + juce::String (dispLimGr, 1) + " dB   (Teto: " + juce::String (ceilDb, 1) + " dBFS)",
                area.removeFromTop (18), juce::Justification::centredLeft);

    auto limBar = area.removeFromTop (24).toFloat();
    g.setColour (bg);
    g.fillRoundedRectangle (limBar, 3.0f);
    const float limNorm = juce::jlimit (0.0f, 1.0f, dispLimGr / 12.0f);
    g.setColour (dispLimGr > 6.0f ? redCol : accent);
    g.fillRoundedRectangle (limBar.withWidth (limBar.getWidth() * limNorm), 3.0f);
}
