#include "../Source/Dsp.h"
#include <cstdio>
#include <cstdlib>
#include <random>

using namespace leve;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("FALHOU: %s\n", msg); ++failures; } } while (0)

int main()
{
    const double sr = 48000.0;
    std::mt19937 rng (1234);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);

    // --- Limiter: sinal forte com picos, em vários tetos ---
    for (float ceilDb : { -0.1f, -1.0f, -3.0f, -6.0f })
    {
        LookaheadLimiter lim; lim.prepare (sr); lim.setReleaseMs (100.0f);
        const float ceiling = dbToGain (ceilDb);
        float worst = 0.0f;
        for (int i = 0; i < 480000; ++i)
        {
            float burst = (i % 9000 < 40) ? 4.0f : 1.0f;                 // picos de +12 dB
            float l = noise (rng) * 1.5f * burst + 0.8f * std::sin (i * 0.05f);
            float r = noise (rng) * 1.5f * burst;
            lim.process (l, r, ceiling, true);
            worst = std::max ({ worst, std::abs (l), std::abs (r) });
        }
        std::printf ("limiter teto %.1f dB -> pico medido %.3f dB\n", ceilDb, gainToDb (worst));
        CHECK (worst <= ceiling + 1.0e-6f, "limiter passou do teto");
    }

    // --- Limiter desativado: só atraso, sinal idêntico ---
    {
        LookaheadLimiter lim; lim.prepare (sr);
        const int d = lim.getLatencySamples();
        std::vector<float> in (2000), out (2000);
        for (int i = 0; i < 2000; ++i)
        {
            in[(size_t) i] = noise (rng) * 0.5f;
            float l = in[(size_t) i], r = l;
            lim.process (l, r, 1.0f, false);
            out[(size_t) i] = l;
        }
        bool same = true;
        for (int i = d; i < 2000; ++i) same = same && std::abs (out[(size_t) i] - in[(size_t) (i - d)]) < 1.0e-6f;
        CHECK (same, "limiter bypass deveria só atrasar");
    }

    // --- Compressor: tom a -6 dBFS, thr -18, ratio 4 => GR ~ 9 dB (menos o joelho) ---
    {
        LinkedCompressor c; c.prepare (sr); c.setParams (-18.0f, 4.0f, 5.0f, 100.0f);
        float amp = dbToGain (-6.0f);
        for (int i = 0; i < 48000; ++i) c.processFrame (amp, amp);
        std::printf ("compressor GR estável: %.2f dB (esperado ~9)\n", c.getGrDb());
        CHECK (std::abs (c.getGrDb() - 9.0f) < 0.2f, "GR do compressor fora do esperado");

        float quiet = dbToGain (-40.0f);
        for (int i = 0; i < 48000; ++i) c.processFrame (quiet, quiet);
        CHECK (c.getGrDb() < 0.05f, "compressor deveria soltar em sinal baixo");
    }

    // --- EQ: +6 dB no grave => ganho ~ +6 dB em 30 Hz; 0 dB => transparente ---
    {
        Eq3 eq; eq.prepare (sr); eq.setGains (6.0f, 0.0f, 0.0f);
        float peak = 0.0f;
        for (int i = 0; i < 96000; ++i)
        {
            float l = 0.1f * std::sin (2.0 * M_PI * 30.0 * i / sr), r = l;
            eq.process (l, r);
            if (i > 48000) peak = std::max (peak, std::abs (l));
        }
        const float gainDb = gainToDb (peak / 0.1f);
        std::printf ("EQ grave @30Hz com +6 dB: %.2f dB\n", gainDb);
        CHECK (gainDb > 5.0f && gainDb < 6.3f, "ganho do shelf grave fora do esperado");

        Eq3 flat; flat.prepare (sr); flat.setGains (0.0f, 0.0f, 0.0f);
        float maxErr = 0.0f;
        for (int i = 0; i < 4800; ++i)
        {
            float x = noise (rng) * 0.5f, l = x, r = x;
            flat.process (l, r);
            maxErr = std::max (maxErr, std::abs (l - x));
        }
        CHECK (maxErr < 1.0e-5f, "EQ a 0 dB deveria ser transparente");
    }

    // --- Saturação: ganho unitário em sinal pequeno ---
    {
        const float x = 0.001f;
        CHECK (std::abs (saturate (x, 1.0f) - x) < 1.0e-5f, "saturador deveria ter ganho unitário em sinal pequeno");
        CHECK (saturate (0.0f, 1.0f) == 0.0f, "saturador sem DC");
    }

    std::printf (failures == 0 ? "TUDO OK\n" : "HÁ FALHAS\n");
    return failures == 0 ? 0 : 1;
}
