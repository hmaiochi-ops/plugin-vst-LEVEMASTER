#pragma once
#include <JuceHeader.h>
#include "Dsp.h"

namespace ids
{
    inline constexpr const char* bypass    = "bypass";
    inline constexpr const char* inGain    = "in_gain";
    inline constexpr const char* bypEq     = "byp_eq";
    inline constexpr const char* bass      = "eq_bass";
    inline constexpr const char* mid       = "eq_mid";
    inline constexpr const char* treble    = "eq_treble";
    inline constexpr const char* bypComp   = "byp_comp";
    inline constexpr const char* thresh    = "comp_thresh";
    inline constexpr const char* ratio     = "comp_ratio";
    inline constexpr const char* attack    = "comp_attack";
    inline constexpr const char* release   = "comp_release";
    inline constexpr const char* mix       = "comp_mix";
    inline constexpr const char* bypSat    = "byp_sat";
    inline constexpr const char* drive     = "sat_drive";
    inline constexpr const char* bypWidth  = "byp_width";
    inline constexpr const char* width     = "width";
    inline constexpr const char* bypLim    = "byp_lim";
    inline constexpr const char* push      = "push";
    inline constexpr const char* ceiling   = "ceiling";
    inline constexpr const char* limRel    = "lim_release";
    inline constexpr const char* outGain   = "out_gain";
}

class LeveMasterProcessor : public juce::AudioProcessor
{
public:
    LeveMasterProcessor();
    ~LeveMasterProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override
    {
        return apvts.getParameter (ids::bypass);
    }

    juce::AudioProcessorValueTreeState apvts;

    // medidores lidos pela interface (valores em dB)
    std::atomic<float> meterCompGr { 0.0f }, meterLimGr { 0.0f }, meterPeak { -120.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::atomic<float>* raw (const char* id) { return apvts.getRawParameterValue (id); }

    // parâmetros (ponteiros para os valores atômicos)
    std::atomic<float> *pIn, *pBypEq, *pBass, *pMid, *pTreble, *pBypComp, *pThresh, *pRatio, *pAttack,
                       *pRelease, *pMix, *pBypSat, *pDrive, *pBypWidth, *pWidth, *pBypLim, *pPush,
                       *pCeiling, *pLimRel, *pOut;

    double sampleRate = 44100.0;
    int maxBlockSize = 512;

    leve::Eq3 eq;
    leve::LinkedCompressor comp;
    leve::LookaheadLimiter limiter;
    juce::dsp::Oversampling<float> oversampler { 2, 1,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };

    juce::SmoothedValue<float> inSmooth, pushSmooth, outSmooth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LeveMasterProcessor)
};
