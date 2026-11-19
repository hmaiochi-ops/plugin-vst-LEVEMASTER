#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

namespace
{
    using Param = std::unique_ptr<juce::RangedAudioParameter>;

    Param makeFloat (const char* id, const juce::String& name, float lo, float hi, float step, float def,
                     const juce::String& unit, int decimals, float skew = 1.0f)
    {
        juce::NormalisableRange<float> range (lo, hi, step, skew);
        auto attrs = juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([unit, decimals] (float v, int)
            {
                return juce::String (v, decimals) + (unit.isEmpty() ? "" : " " + unit);
            })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
        return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def, attrs);
    }

    Param makeBool (const char* id, const juce::String& name)
    {
        return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, false);
    }
}

APVTS::ParameterLayout LeveMasterProcessor::createLayout()
{
    APVTS::ParameterLayout l;

    l.add (makeBool  (ids::bypass,   juce::CharPointer_UTF8 ("Bypass geral")));
    l.add (makeFloat (ids::inGain,   juce::CharPointer_UTF8 ("Entrada"),        -12.0f, 12.0f, 0.1f,   0.0f, "dB", 1));

    l.add (makeBool  (ids::bypEq,    juce::CharPointer_UTF8 ("EQ bypass")));
    l.add (makeFloat (ids::bass,     juce::CharPointer_UTF8 ("Grave"),          -6.0f,  6.0f, 0.1f,    0.0f, "dB", 1));
    l.add (makeFloat (ids::mid,      juce::CharPointer_UTF8 ("Médio"),          -4.0f,  4.0f, 0.1f,    0.0f, "dB", 1));
    l.add (makeFloat (ids::treble,   juce::CharPointer_UTF8 ("Agudo"),          -6.0f,  6.0f, 0.1f,    0.0f, "dB", 1));

    l.add (makeBool  (ids::bypComp,  juce::CharPointer_UTF8 ("Compressor bypass")));
    l.add (makeFloat (ids::thresh,   juce::CharPointer_UTF8 ("Threshold"),      -30.0f, 0.0f, 0.1f,  -12.0f, "dB", 1));
    l.add (makeFloat (ids::ratio,    juce::CharPointer_UTF8 ("Ratio"),          1.1f,   8.0f, 0.01f,   2.0f, ":1", 2, 0.5f));
    l.add (makeFloat (ids::attack,   juce::CharPointer_UTF8 ("Ataque"),         1.0f, 100.0f, 0.1f,   30.0f, "ms", 1, 0.5f));
    l.add (makeFloat (ids::release,  juce::CharPointer_UTF8 ("Soltura"),        20.0f, 800.0f, 1.0f, 200.0f, "ms", 0, 0.5f));
    l.add (makeFloat (ids::mix,      juce::CharPointer_UTF8 ("Mix paralelo"),   0.0f, 100.0f, 1.0f,  100.0f, "%", 0));

    l.add (makeBool  (ids::bypSat,   juce::CharPointer_UTF8 ("Saturação bypass")));
    l.add (makeFloat (ids::drive,    juce::CharPointer_UTF8 ("Drive"),          0.0f,  10.0f, 0.1f,    0.0f, "", 1));

    l.add (makeBool  (ids::bypWidth, juce::CharPointer_UTF8 ("Estéreo bypass")));
    l.add (makeFloat (ids::width,    juce::CharPointer_UTF8 ("Largura"),        0.0f, 200.0f, 1.0f,  100.0f, "%", 0));

    l.add (makeBool  (ids::bypLim,   juce::CharPointer_UTF8 ("Limiter bypass")));
    l.add (makeFloat (ids::push,     juce::CharPointer_UTF8 ("Push"),           0.0f,  12.0f, 0.1f,    0.0f, "dB", 1));
    l.add (makeFloat (ids::ceiling,  juce::CharPointer_UTF8 ("Teto"),           -6.0f,  0.0f, 0.1f,   -1.0f, "dB", 1));
    l.add (makeFloat (ids::limRel,   juce::CharPointer_UTF8 ("Soltura limiter"), 10.0f, 500.0f, 1.0f, 100.0f, "ms", 0, 0.5f));

    l.add (makeFloat (ids::outGain,  juce::CharPointer_UTF8 ("Saída"),          -12.0f, 12.0f, 0.1f,   0.0f, "dB", 1));
    return l;
}

//==============================================================================
LeveMasterProcessor::LeveMasterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LEVEMASTER", createLayout())
{
    pIn = raw (ids::inGain);  pBypEq = raw (ids::bypEq);  pBass = raw (ids::bass);  pMid = raw (ids::mid);
    pTreble = raw (ids::treble);  pBypComp = raw (ids::bypComp);  pThresh = raw (ids::thresh);
    pRatio = raw (ids::ratio);  pAttack = raw (ids::attack);  pRelease = raw (ids::release);
    pMix = raw (ids::mix);  pBypSat = raw (ids::bypSat);  pDrive = raw (ids::drive);
    pBypWidth = raw (ids::bypWidth);  pWidth = raw (ids::width);  pBypLim = raw (ids::bypLim);
    pPush = raw (ids::push);  pCeiling = raw (ids::ceiling);  pLimRel = raw (ids::limRel);
    pOut = raw (ids::outGain);
}

bool LeveMasterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void LeveMasterProcessor::prepareToPlay (double sr, int numSamplesPerBlock)
{
    sampleRate = sr;
    maxBlockSize = std::max (1, numSamplesPerBlock);

    eq.prepare (sr);
    comp.prepare (sr);
    limiter.prepare (sr, 1.5f);
    lufsMeter.prepare (sr);

    oversampler.initProcessing ((size_t) maxBlockSize);
    oversampler.reset();

    setLatencySamples (limiter.getLatencySamples() + (int) std::lround (oversampler.getLatencyInSamples()));

    inSmooth.reset (sr, 0.03);   inSmooth.setCurrentAndTargetValue (leve::dbToGain (pIn->load()));
    pushSmooth.reset (sr, 0.03); pushSmooth.setCurrentAndTargetValue (leve::dbToGain (pPush->load()));
    outSmooth.reset (sr, 0.03);  outSmooth.setCurrentAndTargetValue (leve::dbToGain (pOut->load()));
}

//==============================================================================
void LeveMasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (resetLufsFlag.exchange (false))
        lufsMeter.reset();

    const int total = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || total == 0)
        return;

    const bool bypEq = pBypEq->load() > 0.5f,  bypComp = pBypComp->load() > 0.5f,
               bypSat = pBypSat->load() > 0.5f, bypWidth = pBypWidth->load() > 0.5f,
               bypLim = pBypLim->load() > 0.5f;

    eq.setGains (pBass->load(), pMid->load(), pTreble->load());

    const float thr = pThresh->load(), ratio = pRatio->load();
    comp.setParams (thr, ratio, pAttack->load(), pRelease->load());
    if (bypComp) comp.reset();
    const float mix = pMix->load() * 0.01f;
    const float makeup = leve::dbToGain (std::max (0.0f, -thr * (1.0f - 1.0f / ratio) * 0.5f));

    limiter.setReleaseMs (pLimRel->load());
    const float ceiling = leve::dbToGain (pCeiling->load());
    const float drive01 = bypSat ? 0.0f : pDrive->load() * 0.1f;
    const float width = pWidth->load() * 0.01f;

    inSmooth.setTargetValue (leve::dbToGain (pIn->load()));
    pushSmooth.setTargetValue (leve::dbToGain (pPush->load()));
    outSmooth.setTargetValue (leve::dbToGain (pOut->load()));

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    float maxCompGr = 0.0f, maxLimGr = 0.0f, peakL = 0.0f, peakR = 0.0f, inPeak = 0.0f;

    // Estágio A: entrada -> EQ -> compressor
    for (int i = 0; i < total; ++i)
    {
        const float ig = inSmooth.getNextValue();
        float l = L[i] * ig, r = R[i] * ig;

        inPeak = std::max ({ inPeak, std::abs (l), std::abs (r) });

        if (! bypEq)
            eq.process (l, r);

        if (! bypComp)
        {
            const float g = comp.processFrame (l, r);
            maxCompGr = std::max (maxCompGr, comp.getGrDb());
            const float wetGain = g * makeup;
            l = l * (1.0f - mix) + l * wetGain * mix;
            r = r * (1.0f - mix) + r * wetGain * mix;
        }

        L[i] = l;
        R[i] = r;
    }

    // Estágio B: saturação com oversampling 2x
    for (int start = 0; start < total; start += maxBlockSize)
    {
        const int len = std::min (maxBlockSize, total - start);
        float* ptrs[2] = { L + start, R + start };
        juce::dsp::AudioBlock<float> block (ptrs, 2, (size_t) len);

        auto up = oversampler.processSamplesUp (block);
        if (drive01 > 0.0f)
        {
            for (size_t ch = 0; ch < 2; ++ch)
            {
                float* d = up.getChannelPointer (ch);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                    d[i] = leve::saturate (d[i], drive01);
            }
        }
        oversampler.processSamplesDown (block);
    }

    // Estágio C: largura -> push -> limiter -> saída -> medidores LUFS
    for (int i = 0; i < total; ++i)
    {
        float l = L[i], r = R[i];

        if (! bypWidth && width != 1.0f)
        {
            const float m = (l + r) * 0.5f, s = (l - r) * 0.5f * width;
            l = m + s;
            r = m - s;
        }

        const float pg = pushSmooth.getNextValue();
        if (! bypLim) { l *= pg; r *= pg; }

        limiter.process (l, r, ceiling, ! bypLim);
        maxLimGr = std::max (maxLimGr, limiter.getGrDb());

        const float og = outSmooth.getNextValue();
        l *= og; r *= og;

        lufsMeter.processSample (l, r);

        peakL = std::max (peakL, std::abs (l));
        peakR = std::max (peakR, std::abs (r));
        L[i] = l;
        R[i] = r;
    }

    const float peak = std::max (peakL, peakR);
    meterInLevel.store (leve::gainToDb (inPeak));
    meterCompGr.store (maxCompGr);
    meterLimGr.store (maxLimGr);
    meterPeakL.store (leve::gainToDb (peakL));
    meterPeakR.store (leve::gainToDb (peakR));
    meterPeak.store (leve::gainToDb (peak));
    meterLufsMom.store (lufsMeter.getMomentary());
    meterLufsShort.store (lufsMeter.getShortTerm());
    meterLufsInt.store (lufsMeter.getIntegrated());
    meterPhase.store (lufsMeter.getPhase());
}

//==============================================================================
juce::AudioProcessorEditor* LeveMasterProcessor::createEditor()
{
    return new LeveMasterEditor (*this);
}

void LeveMasterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void LeveMasterProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LeveMasterProcessor();
}
