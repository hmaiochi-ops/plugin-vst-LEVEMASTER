#pragma once
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

namespace leve
{
inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

//==============================================================================
// Biquad (RBJ cookbook), coeficientes e resposta em frequência
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    void setPeak (double sr, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = 2.0 * M_PI * f / sr, c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        normalise (1.0 + alpha * A, -2.0 * c, 1.0 - alpha * A,
                   1.0 + alpha / A, -2.0 * c, 1.0 - alpha / A);
    }

    void setLowShelf (double sr, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = 2.0 * M_PI * f / sr, c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        const double t = 2.0 * std::sqrt (A) * alpha;
        normalise (A * ((A + 1) - (A - 1) * c + t),
                   2.0 * A * ((A - 1) - (A + 1) * c),
                   A * ((A + 1) - (A - 1) * c - t),
                   (A + 1) + (A - 1) * c + t,
                   -2.0 * ((A - 1) + (A + 1) * c),
                   (A + 1) + (A - 1) * c - t);
    }

    void setHighShelf (double sr, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = 2.0 * M_PI * f / sr, c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        const double t = 2.0 * std::sqrt (A) * alpha;
        normalise (A * ((A + 1) + (A - 1) * c + t),
                   -2.0 * A * ((A - 1) + (A + 1) * c),
                   A * ((A + 1) + (A - 1) * c - t),
                   (A + 1) - (A - 1) * c + t,
                   2.0 * ((A - 1) - (A + 1) * c),
                   (A + 1) - (A - 1) * c - t);
    }

    // Coeficientes exatos do filtro K-weighting ITU-R BS.1770-4
    void setKWeightingStage1 (double sr)
    {
        const double db = 3.999843853973347;
        const double f0 = 1681.974450955533;
        const double Q  = 0.7071752369554196;
        const double K  = std::tan (M_PI * f0 / sr);
        const double Vh = std::pow (10.0, db / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        normalise ((Vh + Vb * K / Q + K * K),
                   2.0 * (K * K - Vh),
                   (Vh - Vb * K / Q + K * K),
                   a0,
                   2.0 * (K * K - 1.0),
                   (1.0 - K / Q + K * K));
    }

    void setKWeightingStage2 (double sr)
    {
        const double f0 = 38.13547087602444;
        const double Q  = 0.5003270373238773;
        const double K  = std::tan (M_PI * f0 / sr);
        const double a0 = 1.0 + K / Q + K * K;
        normalise (1.0, -2.0, 1.0,
                   a0,
                   2.0 * (K * K - 1.0),
                   (1.0 - K / Q + K * K));
    }

    double getMagnitudeDb (double sr, double freq) const
    {
        const double w = 2.0 * M_PI * freq / sr;
        const std::complex<double> z1 = std::polar (1.0, -w);
        const std::complex<double> z2 = std::polar (1.0, -2.0 * w);
        const std::complex<double> num = b0 + b1 * z1 + b2 * z2;
        const std::complex<double> den = 1.0 + a1 * z1 + a2 * z2;
        const double mag = std::abs (num / den);
        return 20.0 * std::log10 (std::max (mag, 1.0e-9));
    }

private:
    void normalise (double nb0, double nb1, double nb2, double a0, double na1, double na2)
    {
        b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
    }
};

struct BiquadState
{
    double z1 = 0, z2 = 0;
    float process (const Biquad& c, float x)
    {
        const double in = x;
        const double y = c.b0 * in + z1;
        z1 = c.b1 * in - c.a1 * y + z2;
        z2 = c.b2 * in - c.a2 * y;
        return (float) y;
    }
    void reset() { z1 = z2 = 0; }
};

//==============================================================================
// EQ de 3 bandas: shelf grave, sino médio, shelf agudo (estéreo)
class Eq3
{
public:
    void prepare (double sr) { sampleRate = sr; lastBass = lastMid = lastTreble = 1.0e9f; reset(); }
    void reset() { for (auto& c : st) for (auto& s : c) s.reset(); }

    void setGains (float bassDb, float midDb, float trebleDb)
    {
        if (bassDb != lastBass)     { bass.setLowShelf (sampleRate, 100.0, 0.7, bassDb);     lastBass = bassDb; }
        if (midDb != lastMid)       { mid.setPeak (sampleRate, 2500.0, 0.8, midDb);          lastMid = midDb; }
        if (trebleDb != lastTreble) { treble.setHighShelf (sampleRate, 10000.0, 0.7, trebleDb); lastTreble = trebleDb; }
    }

    void process (float& l, float& r)
    {
        l = st[0][0].process (bass, l);  r = st[1][0].process (bass, r);
        l = st[0][1].process (mid, l);   r = st[1][1].process (mid, r);
        l = st[0][2].process (treble, l); r = st[1][2].process (treble, r);
    }

    // Calcula resposta estática combinada para o gráfico 20Hz-20kHz
    static float computeCombinedDb (double sr, double freq, float bassDb, float midDb, float trebleDb, bool bypassed)
    {
        if (bypassed) return 0.0f;
        Biquad b, m, t;
        b.setLowShelf (sr, 100.0, 0.7, bassDb);
        m.setPeak (sr, 2500.0, 0.8, midDb);
        t.setHighShelf (sr, 10000.0, 0.7, trebleDb);
        return (float) (b.getMagnitudeDb (sr, freq) + m.getMagnitudeDb (sr, freq) + t.getMagnitudeDb (sr, freq));
    }

private:
    double sampleRate = 44100.0;
    Biquad bass, mid, treble;
    BiquadState st[2][3];
    float lastBass = 1.0e9f, lastMid = 1.0e9f, lastTreble = 1.0e9f;
};

//==============================================================================
// Medidor LUFS (ITU-R BS.1770-4) e Correlação de Fase
class LufsMeter
{
public:
    void prepare (double sr)
    {
        sampleRate = std::max (8000.0, sr);
        kw1.setKWeightingStage1 (sampleRate);
        kw2.setKWeightingStage2 (sampleRate);
        stepSamples = std::max (1, (int) std::lround (sampleRate * 0.1)); // blocos de 100ms
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c) { st1[c].reset(); st2[c].reset(); }
        ring.fill (0.0);
        ringIdx = 0;
        blockEnergyAcc = 0.0;
        blockSampleCount = 0;
        integratedEnergySum = 0.0;
        integratedBlockCount = 0;
        momentaryLufs = -70.0f;
        shortTermLufs = -70.0f;
        integratedLufs = -70.0f;
        phaseCorr = 1.0f;
    }

    void processSample (float l, float r)
    {
        // Correlação de fase suavizada
        const double lr = (double) l * (double) r;
        const double l2 = (double) l * (double) l;
        const double r2 = (double) r * (double) r;
        const double alpha = 0.0005;
        covLR = (1.0 - alpha) * covLR + alpha * lr;
        varL  = (1.0 - alpha) * varL  + alpha * l2;
        varR  = (1.0 - alpha) * varR  + alpha * r2;
        const double denom = std::sqrt (varL * varR);
        phaseCorr = denom > 1.0e-12 ? (float) std::clamp (covLR / denom, -1.0, 1.0) : 1.0f;

        // Filtro K-weighting
        float kl = st2[0].process (kw2, st1[0].process (kw1, l));
        float kr = st2[1].process (kw2, st1[1].process (kw1, r));

        blockEnergyAcc += (double) kl * (double) kl + (double) kr * (double) kr;
        if (++blockSampleCount >= stepSamples)
        {
            const double meanSq = blockEnergyAcc / (double) blockSampleCount;
            ring[(size_t) ringIdx] = meanSq;
            ringIdx = (ringIdx + 1) % 30;
            blockEnergyAcc = 0.0;
            blockSampleCount = 0;

            // Momentary = últimos 4 blocos (400 ms)
            double sumM = 0.0;
            for (int k = 0; k < 4; ++k)
                sumM += ring[(size_t) ((ringIdx - 1 - k + 30) % 30)];
            const double eM = sumM / 4.0;
            momentaryLufs = eM > 1.0e-10 ? (float) (-0.691 + 10.0 * std::log10 (eM)) : -70.0f;

            // Short-Term = últimos 30 blocos (3.0 s)
            double sumS = 0.0;
            for (int k = 0; k < 30; ++k)
                sumS += ring[(size_t) k];
            const double eS = sumS / 30.0;
            shortTermLufs = eS > 1.0e-10 ? (float) (-0.691 + 10.0 * std::log10 (eS)) : -70.0f;

            // Integrated com gate absoluto (-70 LUFS)
            if (momentaryLufs > -70.0f)
            {
                integratedEnergySum += eM;
                ++integratedBlockCount;
                const double eI = integratedEnergySum / (double) integratedBlockCount;
                integratedLufs = eI > 1.0e-10 ? (float) (-0.691 + 10.0 * std::log10 (eI)) : -70.0f;
            }
        }
    }

    float getMomentary() const  { return std::max (-70.0f, momentaryLufs); }
    float getShortTerm() const  { return std::max (-70.0f, shortTermLufs); }
    float getIntegrated() const { return std::max (-70.0f, integratedLufs); }
    float getPhase() const      { return phaseCorr; }

private:
    double sampleRate = 44100.0;
    Biquad kw1, kw2;
    BiquadState st1[2], st2[2];
    std::array<double, 30> ring {};
    int ringIdx = 0, stepSamples = 4410, blockSampleCount = 0;
    double blockEnergyAcc = 0.0, integratedEnergySum = 0.0;
    long long integratedBlockCount = 0;
    double covLR = 0.0, varL = 0.0, varR = 0.0;
    float momentaryLufs = -70.0f, shortTermLufs = -70.0f, integratedLufs = -70.0f, phaseCorr = 1.0f;
};

//==============================================================================
// Compressor feed-forward, detecção linkada (L/R juntos), joelho suave de 6 dB
class LinkedCompressor
{
public:
    void prepare (double sr) { sampleRate = sr; grDb = 0.0f; setTimes (attackMs, releaseMs); }
    void reset() { grDb = 0.0f; }

    void setParams (float thresholdDb, float ratioIn, float attackMsIn, float releaseMsIn)
    {
        threshold = thresholdDb;
        ratio = std::max (1.0f, ratioIn);
        if (attackMsIn != attackMs || releaseMsIn != releaseMs) setTimes (attackMsIn, releaseMsIn);
    }

    float getGrDb() const { return grDb; }

    // Calcula curva estática de transferência para o gráfico do compressor
    static float computeTransferDb (float inputDb, float thresholdDb, float ratioIn, float mix01, bool bypassed)
    {
        if (bypassed) return inputDb;
        const float r = std::max (1.0f, ratioIn);
        const float knee = 6.0f;
        const float over = inputDb - thresholdDb;
        float gr = 0.0f;
        if (2.0f * over > knee)
            gr = over * (1.0f - 1.0f / r);
        else if (2.0f * over > -knee)
        {
            const float x = over + knee * 0.5f;
            gr = (1.0f - 1.0f / r) * x * x / (2.0f * knee);
        }
        const float makeupDb = std::max (0.0f, -thresholdDb * (1.0f - 1.0f / r) * 0.5f);
        const float wetOutDb = inputDb - gr + makeupDb;
        const float dryLin = dbToGain (inputDb);
        const float wetLin = dbToGain (wetOutDb);
        return gainToDb (dryLin * (1.0f - mix01) + wetLin * mix01);
    }

    float processFrame (float l, float r)
    {
        const float level = gainToDb (std::max (std::abs (l), std::abs (r)));
        const float over = level - threshold;
        float target = 0.0f;
        if (2.0f * over > knee)
            target = over * (1.0f - 1.0f / ratio);
        else if (2.0f * over > -knee)
        {
            const float x = over + knee * 0.5f;
            target = (1.0f - 1.0f / ratio) * x * x / (2.0f * knee);
        }
        const float coef = target > grDb ? attackCoef : releaseCoef;
        grDb = coef * grDb + (1.0f - coef) * target;
        return dbToGain (-grDb);
    }

private:
    void setTimes (float a, float r)
    {
        attackMs = a; releaseMs = r;
        attackCoef  = std::exp (-1.0f / std::max (1.0f, (float) (a * 0.001 * sampleRate)));
        releaseCoef = std::exp (-1.0f / std::max (1.0f, (float) (r * 0.001 * sampleRate)));
    }

    double sampleRate = 44100.0;
    float threshold = -12.0f, ratio = 2.0f, knee = 6.0f;
    float attackMs = 30.0f, releaseMs = 200.0f;
    float attackCoef = 0.0f, releaseCoef = 0.0f;
    float grDb = 0.0f;
};

//==============================================================================
// Limiter brickwall com lookahead (~1,5 ms)
class LookaheadLimiter
{
public:
    void prepare (double sr, float lookaheadMs = 1.5f)
    {
        sampleRate = sr;
        N = std::max (2, (int) std::lround (sr * lookaheadMs * 0.001));
        delayL.assign ((size_t) N, 0.0f);
        delayR.assign ((size_t) N, 0.0f);
        req.assign ((size_t) N, 1.0f);
        mins.assign ((size_t) N, 1.0f);
        reset();
        setReleaseMs (releaseMs);
    }

    void reset()
    {
        std::fill (delayL.begin(), delayL.end(), 0.0f);
        std::fill (delayR.begin(), delayR.end(), 0.0f);
        std::fill (req.begin(), req.end(), 1.0f);
        std::fill (mins.begin(), mins.end(), 1.0f);
        sum = (double) N; pos = 0; gs = 1.0f; grDb = 0.0f;
    }

    int getLatencySamples() const { return N - 1; }
    float getGrDb() const { return grDb; }

    void setReleaseMs (float ms)
    {
        releaseMs = ms;
        releaseCoef = std::exp (-1.0f / std::max (1.0f, (float) (ms * 0.001 * sampleRate)));
    }

    void process (float& l, float& r, float ceiling, bool enabled)
    {
        const float peak = std::max (std::abs (l), std::abs (r));
        const float need = (enabled && peak > ceiling) ? ceiling / peak : 1.0f;

        req[(size_t) pos] = need;
        float m = 1.0f;
        for (float v : req) m = std::min (m, v);

        sum -= (double) mins[(size_t) pos];
        mins[(size_t) pos] = m;
        sum += (double) m;
        const float avg = (float) (sum / (double) N);

        if (avg < gs) gs = avg;
        else          gs = avg + releaseCoef * (gs - avg);

        delayL[(size_t) pos] = l;
        delayR[(size_t) pos] = r;
        const int readPos = (pos + 1) % N;
        float outL = delayL[(size_t) readPos] * gs;
        float outR = delayR[(size_t) readPos] * gs;
        pos = readPos;

        if (enabled)
        {
            outL = std::clamp (outL, -ceiling, ceiling);
            outR = std::clamp (outR, -ceiling, ceiling);
        }
        l = outL; r = outR;
        grDb = -gainToDb (gs);
    }

private:
    double sampleRate = 44100.0;
    int N = 66, pos = 0;
    std::vector<float> delayL, delayR, req, mins;
    double sum = 0.0;
    float gs = 1.0f, grDb = 0.0f, releaseMs = 100.0f, releaseCoef = 0.0f;
};

//==============================================================================
// Saturação suave (ganho unitário em sinais pequenos). drive01: 0..1
inline float saturate (float x, float drive01)
{
    constexpr float k = 1.5f;
    return x * (1.0f - drive01) + drive01 * std::tanh (k * x) * (1.0f / k);
}

} // namespace leve
