// RevoloDSP.h — moteur audio pur C++ (aucune dépendance JUCE), testable en ligne de commande.
//
// Chaîne (d'après le schéma du manuel TEIL2 Revolo) :
//   IN → [+ NOISE] → porte MOM/ONF → FILTER (low-cut) → REVERB (CHARACTER/LENGTH/AIR) → TREMOLO → OUT
//   Le chemin sec (MIX) est ajouté APRÈS le trémolo ; en mode WET seul le signal traité sort.
//
// Objectif sonore (fiche produit Teile) : « a large, airy soundscape that sits above a mix, instead of
// muddying it up ». La réverb est donc volontairement maigre dans le grave et brillante dans l'aigu :
//   - pré-délai court, réflexions précoces + diffusion d'entrée,
//   - FDN à 12 lignes (matrice de Householder, lignes modulées) pour une queue dense et lisse,
//   - dans la boucle : amortissement des aigus (AIR) ET étagère grave qui fait mourir le bas 3-4x plus vite
//     que le médium (pas d'accumulation de graves),
//   - passe-haut fixe sur la sortie de la réverb, léger « air » (shelf aigu) piloté par AIR,
//   - bruit blanc passe-haut (sans grave) injecté dans la réverb, puis trémolo sur réverb + bruit.
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

// Passe-haut 2e ordre (SVF TPT) : fréquence modulable à l'échantillon, ou coefficients fixes
struct HighPass
{
    float ic1 = 0.f, ic2 = 0.f;
    float a1 = 0.f, a2 = 0.f, a3 = 0.f;
    static constexpr float k = 1.41421356f;   // Butterworth

    void clear() { ic1 = ic2 = 0.f; }
    void setFixed (float fc, float sr)
    {
        fc = std::min (fc, 0.45f * sr);
        const float g = std::tan (kPi * fc / sr);
        a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    float processFixed (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        return x - k * v1 - v2;
    }
    float process (float x, float fc, float sr)
    {
        setFixed (fc, sr);
        return processFixed (x);
    }
};

// Limiteur doux (genou en tanh) : ne coupe jamais net, plafonne à 1.0
inline float softLimit (float x)
{
    constexpr float t = 0.85f;
    const float a = std::fabs (x);
    if (a <= t) return x;
    const float s = x < 0.f ? -1.f : 1.f;
    return s * (t + (1.f - t) * std::tanh ((a - t) / (1.f - t)));
}

class Reverb
{
public:
    static constexpr int N = 12;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        const int maxD = (int) (0.20 * sampleRate);
        for (auto& l : lines) l.prepare (maxD);
        for (auto& l : erIn) l.prepare ((int) (0.13 * sampleRate));
        for (auto& l : pre)  l.prepare ((int) (0.03 * sampleRate));
        preSamples = 0.012f * sr;   // pré-délai de 12 ms : la réverb se détache du signal sec

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
            inc[i] = (0.09f + 0.031f * (float) i) / sr;   // LFO lents, ~0,1 – 0,45 Hz
        }
        modDepth = 0.00045f * sr;
        lfCoef  = 1.f - std::exp (-2.f * kPi * kLfHz / sr);
        airCoef = 1.f - std::exp (-2.f * kPi * 5500.f / sr);
        for (auto& h : outHp) h.setFixed (kOutHpHz, sr);

        sizeS.prepare (sampleRate, 250.f);
        rtS.prepare (sampleRate, 120.f);
        dampS.prepare (sampleRate, 60.f);
        diffS.prepare (sampleRate, 120.f);
        airS.prepare (sampleRate, 60.f);
        reset();
        setParams (1.f, 0.72f, 0.5f, true);
    }

    void reset()
    {
        for (auto& l : lines) l.clear();
        for (auto& l : erIn) l.clear();
        for (auto& l : pre) l.clear();
        for (auto& a : apL) a.clear();
        for (auto& a : apR) a.clear();
        for (auto& h : outHp) h.clear();
        for (int i = 0; i < N; ++i) lpD[i] = lpF[i] = 0.f;
        airLp[0] = airLp[1] = 0.f;
    }

    // character, length, air : 0..1
    void setParams (float character, float length, float air, bool jump = false)
    {
        const float scale = 0.45f + 1.15f * character;                  // taille de la pièce
        const float rt60  = kRtCal * 0.25f * std::pow (40.f, length);   // ~0,25 s … ~10 s (milieu du spectre)
        const float fc    = 3500.f * std::pow (20000.f / 3500.f, air);  // amortissement HF dans la boucle
        const float damp  = 1.f - std::exp (-2.f * kPi * fc / sr);
        const float diff  = 0.45f + 0.20f * character;                  // Character = plus dense / épais
        const float airG  = 0.75f * air;                                // « sizzle » HF en sortie
        sizeS.set (scale); rtS.set (rt60); dampS.set (damp); diffS.set (diff); airS.set (airG);
        if (jump) { sizeS.reset (scale); rtS.reset (rt60); dampS.reset (damp); diffS.reset (diff); airS.reset (airG); }
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        const float scale = sizeS.next();
        const float rt = rtS.next();
        const float dc = dampS.next();
        const float dg = diffS.next();
        const float ag = airS.next();

        // réflexions précoces (sur le signal brut)
        erIn[0].push (inL); erIn[1].push (inR);
        float erL = 0.f, erR = 0.f;
        for (int k = 0; k < 6; ++k)
        {
            erL += erGL[k] * erIn[0].read (erMsL[k] * scale * 0.001f * sr);
            erR += erGR[k] * erIn[1].read (erMsR[k] * scale * 0.001f * sr);
        }

        // pré-délai puis diffusion d'entrée
        float a = pre[0].read (preSamples - 1.f), b = pre[1].read (preSamples - 1.f);
        pre[0].push (inL); pre[1].push (inR);
        for (auto& ap : apL) { ap.g = dg; a = ap.process (a); }
        for (auto& ap : apR) { ap.g = dg; b = ap.process (b); }

        // FDN
        float x[N]; float sum = 0.f;
        for (int i = 0; i < N; ++i)
        {
            const float lfo = std::sin (2.f * kPi * phase[i]);
            phase[i] += inc[i]; if (phase[i] >= 1.f) phase[i] -= 1.f;
            const float D = baseSamples[i] * scale + modDepth * lfo;
            float y = lines[i].read (D - 1.f);
            lpD[i] += dc * (y - lpD[i]);              // amortissement des aigus (AIR)
            y = lpD[i];
            lpF[i] += lfCoef * (y - lpF[i]);          // estimation du grave...
            y -= kLfShelf * lpF[i];                   // ...retranchée dans la boucle : le grave décroît bien plus vite
            const float g = std::exp (-6.9078f * (D / sr) / rt);
            x[i] = y * g;
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
        float wL = kOutScale * fL + kErLevel * erL;
        float wR = kOutScale * fR + kErLevel * erR;

        // sortie : coupe-bas fixe + « air »
        wL = outHp[0].processFixed (wL);
        wR = outHp[1].processFixed (wR);
        airLp[0] += airCoef * (wL - airLp[0]);  wL += ag * (wL - airLp[0]);
        airLp[1] += airCoef * (wR - airLp[1]);  wR += ag * (wR - airLp[1]);
        outL = wL; outR = wR;
    }

private:
    static constexpr float kInGain   = 0.35f;
    static constexpr float kOutScale = 0.40825f;   // 1/sqrt(N/2)
    static constexpr float kErLevel  = 0.28f;
    static constexpr float kLfHz     = 250.f;      // fréquence de l'étagère grave dans la boucle
    static constexpr float kLfShelf  = 0.26f;      // 0 = aucun effet ; plus grand = grave qui meurt plus vite
    static constexpr float kOutHpHz  = 70.f;       // coupe-bas fixe de la sortie réverb
    static constexpr float kRtCal    = 1.12f;      // calibre « LENGTH » sur le RT60 mesuré en médium

    float sr = 44100.f, modDepth = 10.f, preSamples = 500.f, lfCoef = 0.f, airCoef = 0.f;
    std::array<DelayLine, N> lines;
    std::array<DelayLine, 2> erIn, pre;
    std::array<AllPass, 4> apL, apR;
    std::array<HighPass, 2> outHp;
    float lpD[N] = {}, lpF[N] = {}, phase[N] = {}, inc[N] = {}, baseSamples[N] = {}, airLp[2] = {};
    Smoothed sizeS, rtS, dampS, diffS, airS;
    const float baseMs[N] = { 27.1f, 31.3f, 35.9f, 41.3f, 46.7f, 52.9f, 58.3f, 64.7f, 71.9f, 79.3f, 87.1f, 95.3f };
    const float erMsL[6] = { 5.3f, 9.1f, 14.7f, 21.9f, 31.3f, 42.7f };
    const float erMsR[6] = { 6.7f, 11.3f, 17.9f, 25.1f, 36.7f, 47.9f };
    const float erGL[6] = { 0.70f, -0.60f, 0.50f, -0.42f, 0.35f, -0.28f };
    const float erGR[6] = { -0.68f, 0.58f, -0.48f, 0.40f, -0.33f, 0.26f };
};

struct Params
{
    float reverb = 1.f;      // 0..1  niveau du signal réverbéré
    float character = 1.f;   // 0..1  taille / densité de la pièce
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
        for (auto& h : noiseHp) h.setFixed (kNoiseHpHz, sr);
        setParams (Params{}, true);
        hpL.clear(); hpR.clear(); lfo = 0.f;
        for (auto& h : noiseHp) h.clear();
    }

    void setParams (const Params& p, bool jump = false)
    {
        reverb.setParams (p.character, p.length, p.air, jump);
        gateS.set (p.active ? 1.f : 0.f);
        levelS.set (p.reverb);
        noiseS.set (p.noise);
        trS.set (p.tremolo);
        const float lc = std::log (std::max (20.f, std::min (20000.f, p.filterHz)));
        cutS.set (lc);
        tempo = std::max (0.2f, std::min (40.f, p.tempoHz));
        mix = p.mixMode;
        if (jump) { gateS.reset (p.active ? 1.f : 0.f); levelS.reset (p.reverb); noiseS.reset (p.noise);
                    trS.reset (p.tremolo); cutS.reset (lc); }
    }

    void processSample (float inL, float inR, float& outL, float& outR)
    {
        const float gate = gateS.next();
        const float nz = noiseS.next() * kNoiseGain;
        const float fc = std::exp (cutS.next());

        // bruit blanc SANS grave (passe-haut fixe), injecté avec le signal ; le tout passe par la porte MOM/ONF
        const float nL = noiseHp[0].processFixed (rnd());
        const float nR = noiseHp[1].processFixed (rnd());
        const float sendL = (inL + nz * nL) * gate;
        const float sendR = (inR + nz * nR) * gate;

        // low-cut FILTER (avant la réverb, comme sur la pédale)
        const float fL = hpL.process (sendL, fc, sr);
        const float fR = hpR.process (sendR, fc, sr);

        float wL, wR;
        reverb.process (fL, fR, wL, wR);
        const float lvl = levelS.next() * kWet;
        wL *= lvl; wR *= lvl;

        // trémolo sur la réverb ET le bruit (effet « gater / side-chain »)
        const float depth = trS.next();
        const float tg = 1.f - depth * (0.5f - 0.5f * std::cos (2.f * kPi * lfo));
        lfo += tempo / sr; if (lfo >= 1.f) lfo -= 1.f;
        wL = softLimit (wL * tg); wR = softLimit (wR * tg);

        outL = wL + (mix ? inL : 0.f);
        outR = wR + (mix ? inR : 0.f);
    }

    float getLfoPhase() const { return lfo; }
    void setLfoPhase (float p) { lfo = p - std::floor (p); }   // synchro tempo / tap

private:
    static constexpr float kWet = 0.5f;           // niveau de la réverb à REVERB = max : sous le signal sec
    static constexpr float kNoiseGain = 0.6f;     // le bruit de la vraie est « fort même à faible dose »
    static constexpr float kNoiseHpHz = 800.f;    // pas de grave dans le bruit

    float rnd() { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.f / 2147483648.f); }
    float sr = 44100.f, lfo = 0.f, tempo = 2.f;
    bool mix = false;
    uint32_t seed = 22222u;
    Reverb reverb;
    HighPass hpL, hpR;
    std::array<HighPass, 2> noiseHp;
    Smoothed gateS, levelS, noiseS, trS, cutS;
};
} // namespace revolo
