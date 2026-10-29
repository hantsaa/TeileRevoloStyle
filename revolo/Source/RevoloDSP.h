// RevoloDSP.h — moteur audio pur C++ (aucune dépendance JUCE), testable en ligne de commande.
// Chaîne (d'après le schéma du manuel TEIL2 Revolo) :
//   IN → [+ NOISE] → porte MOM/ONF → FILTER (low-cut) → REVERB (CHARACTER/LENGTH/AIR) → TREMOLO → OUT
//   Le chemin sec (MIX) est ajouté APRÈS le trémolo ; en mode WET seul le signal traité sort.
#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace revolo
{
constexpr float kPi = 3.14159265358979f;

// Lissage à un pôle
struct Smoothed
{
    float cur = 0.f, target = 0.f, coef = 1.f;
    void prepare (double sr, float ms) { coef = 1.f - std::exp (-1.f / (0.001f * ms * (float) sr)); }
    void reset (float v) { cur = target = v; }
    void set (float v) { target = v; }
    float next() { cur += (target - cur) * coef; return cur; }
};

class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        int n = 8;
        while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.f);
        mask = n - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.f); }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // d = nombre d'échantillons en arrière depuis le dernier échantillon écrit (d >= 0)
    float read (float d) const
    {
        const float pos = (float) (w - 1) - d;
        const int i0 = (int) std::floor (pos);
        const float f = pos - (float) i0;
        const float a = buf[(size_t) (i0 & mask)];
        const float b = buf[(size_t) ((i0 + 1) & mask)];
        return a + (b - a) * f;
    }
private:
    std::vector<float> buf;
    int mask = 7, w = 0;
};

// Passe-tout de Schroeder (retard entier)
struct AllPass
{
    DelayLine dl; int len = 1; float g = 0.5f;
    void prepare (float ms, float gain, double sr)
    {
        len = std::max (2, (int) (ms * 0.001 * sr));
        dl.prepare (len + 2);
        g = gain;
    }
    void clear() { dl.clear(); }
    float process (float in)
    {
        const float d = dl.read ((float) (len - 1));
        const float w = in + g * d;
        const float out = d - g * w;
        dl.push (w);
        return out;
    }
};

// Passe-haut 2e ordre (SVF TPT), fréquence modulable à l'échantillon
struct HighPass
{
    float ic1 = 0.f, ic2 = 0.f;
    void clear() { ic1 = ic2 = 0.f; }
    float process (float x, float fc, float sr)
    {
        fc = std::min (fc, 0.45f * sr);
        const float g = std::tan (kPi * fc / sr);
        const float k = 1.41421356f;
        const float a1 = 1.f / (1.f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        return x - k * v1 - v2;
    }
};

// Réverb : réflexions précoces + diffusion d'entrée + FDN 8 lignes (matrice de Householder)
class Reverb
{
public:
    static constexpr int N = 8;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        const int maxD = (int) (0.17 * sampleRate);
        for (auto& l : lines) l.prepare (maxD);
        for (auto& l : erIn) l.prepare ((int) (0.13 * sampleRate));
        const float apMsL[4] = { 3.2f, 2.4f, 8.6f, 6.3f };
        const float apMsR[4] = { 3.4f, 2.6f, 8.9f, 6.6f };
        for (int i = 0; i < 4; ++i)
        {
            apL[i].prepare (apMsL[i], 0.6f, sampleRate);
            apR[i].prepare (apMsR[i], 0.6f, sampleRate);
        }
        for (int i = 0; i < N; ++i)
        {
            baseSamples[i] = baseMs[i] * 0.001f * sr;
            phase[i] = (float) i / (float) N;
            inc[i] = (0.11f + 0.037f * (float) i) / sr; // LFO lents, 0.1 – 0.4 Hz
        }
        modDepth = 0.0004f * sr;
        sizeS.prepare (sampleRate, 250.f);
        rtS.prepare (sampleRate, 120.f);
        dampS.prepare (sampleRate, 60.f);
        reset();
        setParams (1.f, 0.72f, 0.5f, true);
    }

    void reset()
    {
        for (auto& l : lines) l.clear();
        for (auto& l : erIn) l.clear();
        for (auto& a : apL) a.clear();
        for (auto& a : apR) a.clear();
        for (auto& v : lp) v = 0.f;
    }

    // character, length, air : 0..1
    void setParams (float character, float length, float air, bool jump = false)
    {
        const float scale = 0.35f + 1.25f * character;               // taille de la pièce
        const float rt60 = 0.25f * std::pow (40.f, length);          // 0.25 s … 10 s
        const float fc = 1500.f * std::pow (16000.f / 1500.f, air);  // amortissement HF dans la boucle
        const float a = 1.f - std::exp (-2.f * kPi * fc / sr);
        sizeS.set (scale); rtS.set (rt60); dampS.set (a);
        if (jump) { sizeS.reset (scale); rtS.reset (rt60); dampS.reset (a); }
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        const float scale = sizeS.next();
        const float rt = rtS.next();
        const float dc = dampS.next();

        // diffusion d'entrée
        float a = inL, b = inR;
        for (auto& ap : apL) a = ap.process (a);
        for (auto& ap : apR) b = ap.process (b);

        // réflexions précoces
        erIn[0].push (inL); erIn[1].push (inR);
        float erL = 0.f, erR = 0.f;
        for (int k = 0; k < 6; ++k)
        {
            erL += erGL[k] * erIn[0].read (erMsL[k] * scale * 0.001f * sr);
            erR += erGR[k] * erIn[1].read (erMsR[k] * scale * 0.001f * sr);
        }

        // FDN
        float x[N]; float sum = 0.f;
        for (int i = 0; i < N; ++i)
        {
            const float lfo = std::sin (2.f * kPi * phase[i]);
            phase[i] += inc[i]; if (phase[i] >= 1.f) phase[i] -= 1.f;
            const float D = baseSamples[i] * scale + modDepth * lfo;
            const float y = lines[i].read (D - 1.f);
            lp[i] += dc * (y - lp[i]);
            const float g = std::exp (-6.9078f * (D / sr) / rt);
            x[i] = lp[i] * g;
            sum += x[i];
        }
        const float h = sum * (2.f / (float) N);
        float fL = 0.f, fR = 0.f;
        for (int i = 0; i < N; ++i)
        {
            const float s = ((i >> 1) & 1) ? -1.f : 1.f;
            const float in = ((i & 1) ? b : a) * s * kInGain;
            lines[i].push (x[i] - h + in);
            if (i & 1) fR += s * x[i]; else fL += s * x[i];
        }
        outL = 0.5f * fL + kErLevel * erL;
        outR = 0.5f * fR + kErLevel * erR;
    }

private:
    static constexpr float kInGain = 0.5f;
    static constexpr float kErLevel = 0.30f;
    float sr = 44100.f, modDepth = 10.f;
    std::array<DelayLine, N> lines;
    std::array<DelayLine, 2> erIn;
    std::array<AllPass, 4> apL, apR;
    float lp[N] = {}, phase[N] = {}, inc[N] = {}, baseSamples[N] = {};
    Smoothed sizeS, rtS, dampS;
    const float baseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 61.9f, 71.3f, 79.7f };
    const float erMsL[6] = { 5.3f, 9.1f, 14.7f, 21.9f, 31.3f, 42.7f };
    const float erMsR[6] = { 6.7f, 11.3f, 17.9f, 25.1f, 36.7f, 47.9f };
    const float erGL[6] = { 0.70f, -0.60f, 0.50f, -0.42f, 0.35f, -0.28f };
    const float erGR[6] = { -0.68f, 0.58f, -0.48f, 0.40f, -0.33f, 0.26f };
};

struct Params
{
    float reverb = 1.f;      // 0..1  niveau du signal réverbéré
    float character = 1.f;   // 0..1  taille / caractère de la pièce
    float length = 0.72f;    // 0..1  durée de la queue
    float air = 0.5f;        // 0..1  brillance HF
    float noise = 0.f;       // 0..1  bruit blanc injecté dans la réverb
    float filterHz = 150.f;  // 20..20000 Hz  low-cut
    float tremolo = 0.f;     // 0..1  profondeur
    float tempoHz = 2.f;     // 0.2..40 Hz
    bool  mixMode = false;   // false = WET, true = MIX (sec + traité)
    bool  active = true;     // état du bouton ONF (ou maintenu en MOM)
};

class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        reverb.prepare (sampleRate);
        gateS.prepare (sampleRate, 8.f);
        levelS.prepare (sampleRate, 20.f);
        noiseS.prepare (sampleRate, 20.f);
        trS.prepare (sampleRate, 20.f);
        cutS.prepare (sampleRate, 20.f);
        setParams (Params{}, true);
        hpL.clear(); hpR.clear(); lfo = 0.f;
    }

    void setParams (const Params& p, bool jump = false)
    {
        reverb.setParams (p.character, p.length, p.air, jump);
        gateS.set (p.active ? 1.f : 0.f);
        levelS.set (p.reverb);
        noiseS.set (p.noise);
        trS.set (p.tremolo);
        cutS.set (std::log (std::max (20.f, std::min (20000.f, p.filterHz))));
        tempo = std::max (0.2f, std::min (40.f, p.tempoHz));
        mix = p.mixMode;
        if (jump) { gateS.reset (p.active ? 1.f : 0.f); levelS.reset (p.reverb); noiseS.reset (p.noise);
                    trS.reset (p.tremolo); cutS.reset (std::log (std::max (20.f, std::min (20000.f, p.filterHz)))); }
    }

    void processSample (float inL, float inR, float& outL, float& outR)
    {
        const float gate = gateS.next();
        const float nz = noiseS.next() * 0.5f;
        const float fc = std::exp (cutS.next());

        // bruit blanc (uniforme) injecté avec le signal, le tout passant par la porte MOM/ONF
        const float sendL = (inL + nz * rnd()) * gate;
        const float sendR = (inR + nz * rnd()) * gate;

        // low-cut
        const float fL = hpL.process (sendL, fc, sr);
        const float fR = hpR.process (sendR, fc, sr);

        float wL, wR;
        reverb.process (fL, fR, wL, wR);
        const float lvl = levelS.next();
        wL *= lvl; wR *= lvl;

        // trémolo sur la réverb
        const float depth = trS.next();
        const float tg = 1.f - depth * (0.5f - 0.5f * std::cos (2.f * kPi * lfo));
        lfo += tempo / sr; if (lfo >= 1.f) lfo -= 1.f;
        wL *= tg; wR *= tg;

        outL = wL + (mix ? inL : 0.f);
        outR = wR + (mix ? inR : 0.f);
    }

    float getLfoPhase() const { return lfo; }
    void setLfoPhase (float p) { lfo = p - std::floor (p); }   // synchro tempo / tap

private:
    float rnd() { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.f / 2147483648.f); }
    float sr = 44100.f, lfo = 0.f, tempo = 2.f;
    bool mix = false;
    uint32_t seed = 22222u;
    Reverb reverb;
    HighPass hpL, hpR;
    Smoothed gateS, levelS, noiseS, trS, cutS;
};
} // namespace revolo
