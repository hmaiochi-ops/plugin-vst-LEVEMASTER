#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#pragma once
// DSP do LeveMaster — só biblioteca padrão (sem JUCE), para poder testar isolado.
#include <algorithm>
#include <cmath>
#include <vector>

namespace leve
{
inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

//==============================================================================
// Biquad (RBJ cookbook), coeficientes e estado em double
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

private:
    double sampleRate = 44100.0;
    Biquad bass, mid, treble;
    BiquadState st[2][3];
    float lastBass = 1.0e9f, lastMid = 1.0e9f, lastTreble = 1.0e9f;
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

    float getGrDb() const { return grDb; }   // redução atual, positiva (ex.: 3.0 = -3 dB)

    // devolve o ganho linear a aplicar neste frame estéreo
    float processFrame (float l, float r)
    {
        const float level = gainToDb (std::max (std::abs (l), std::abs (r)));
        const float over = level - threshold;
        float target = 0.0f;                                   // redução desejada em dB (>= 0)
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
// Limiter brickwall com lookahead (~1,5 ms). Garante pico de amostra <= teto.
// (Não garante true-peak entre amostras: deixe uns 0,5–1 dB de folga no teto.)
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
    float getGrDb() const { return grDb; }   // positivo

    void setReleaseMs (float ms)
    {
        releaseMs = ms;
        releaseCoef = std::exp (-1.0f / std::max (1.0f, (float) (ms * 0.001 * sampleRate)));
    }

    // enabled=false: só atrasa o áudio (mantém a latência constante) e deixa o ganho voltar a 1
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
        const int readPos = (pos + 1) % N;          // amostra escrita N-1 passos atrás
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
