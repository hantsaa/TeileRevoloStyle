#include "../Source/RevoloDSP.h"
#include <cstdio>
#include <random>
using namespace revolo;

static double rms (const std::vector<float>& v, size_t a, size_t b)
{ double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt (s / (double) (b - a)); }

int main()
{
    const double sr = 48000.0;
    std::mt19937 rng (1); std::normal_distribution<float> nd (0.f, 0.1f);
    int bad = 0;

    printf ("== Gain en régime établi (bruit blanc -20 dB RMS en entrée), character=1 ==\n");
    for (float len : { 0.2f, 0.5f, 0.72f, 0.9f, 1.0f })
    {
        Engine e; e.prepare (sr); Params p; p.length = len; e.setParams (p, true);
        std::vector<float> in, out; const size_t n = (size_t) (sr * 12);
        for (size_t i = 0; i < n; ++i)
        {
            float x = nd (rng), l, r; e.processSample (x, x * 0.9f, l, r);
            if (!std::isfinite (l) || !std::isfinite (r)) ++bad;
            in.push_back (x); out.push_back (l);
        }
        printf ("length %.2f  RT60 %.2fs : in %.3f  out %.3f  (%.1f dB)\n", len, 0.25 * std::pow (40.0, len),
                rms (in, n - 48000, n), rms (out, n - 48000, n), 20 * std::log10 (rms (out, n - 48000, n) / rms (in, n - 48000, n)));
    }

    printf ("== RT60 mesuré (réponse impulsionnelle, intégration de Schroeder) ==\n");
    for (float len : { 0.3f, 0.72f, 1.0f })
    {
        Engine e; e.prepare (sr); Params p; p.length = len; p.air = 1.f; e.setParams (p, true);
        const size_t n = (size_t) (sr * 25); std::vector<double> h (n);
        for (size_t i = 0; i < n; ++i) { float l, r; e.processSample (i == 0 ? 1.f : 0.f, i == 0 ? 1.f : 0.f, l, r); h[i] = l; }
        std::vector<double> edc (n); double acc = 0; for (size_t i = n; i-- > 0;) { acc += h[i] * h[i]; edc[i] = acc; }
        size_t t5 = 0, t35 = 0; for (size_t i = 0; i < n; ++i) { double db = 10 * std::log10 (edc[i] / edc[0] + 1e-30); if (!t5 && db < -5) t5 = i; if (!t35 && db < -35) { t35 = i; break; } }
        printf ("length %.2f : attendu %.2fs, mesuré (T30 x2) %.2fs\n", len, 0.25 * std::pow (40.0, len), 2.0 * (double) (t35 - t5) / sr);
    }

    printf ("== Porte MOM : active=false => rien en sortie WET ; queue après relâchement ==\n");
    {
        Engine e; e.prepare (sr); Params p; p.active = false; e.setParams (p, true);
        std::vector<float> o; for (int i = 0; i < 48000; ++i) { float l, r; e.processSample (nd (rng), nd (rng), l, r); o.push_back (l); }
        printf ("porte fermée : RMS sortie = %.6f (doit être ~0)\n", rms (o, 24000, 48000));
        p.active = true; e.setParams (p); o.clear();
        for (int i = 0; i < 24000; ++i) { float l, r; e.processSample (nd (rng), nd (rng), l, r); o.push_back (l); }
        p.active = false; e.setParams (p); o.clear();
        for (int i = 0; i < 96000; ++i) { float l, r; e.processSample (nd (rng), nd (rng), l, r); o.push_back (l); }
        printf ("après relâchement : RMS 0-0.5s = %.4f, 1.5-2s = %.4f (la queue doit décroître)\n", rms (o, 0, 24000), rms (o, 72000, 96000));
    }

    printf ("== Stabilité : balayage rapide des réglages ==\n");
    {
        Engine e; e.prepare (sr); Params p; p.noise = 1.f; p.tremolo = 1.f; p.mixMode = true; double peak = 0;
        for (int blk = 0; blk < 4000; ++blk)
        {
            p.character = (float) (blk % 37) / 36.f; p.length = (float) (blk % 23) / 22.f; p.air = (float) (blk % 11) / 10.f;
            p.filterHz = 20.f * std::pow (1000.f, (float) (blk % 29) / 28.f); p.tempoHz = 0.2f + (float) (blk % 13) * 3.f;
            p.active = (blk / 50) % 2 == 0; e.setParams (p);
            for (int i = 0; i < 64; ++i) { float l, r; e.processSample (nd (rng), nd (rng), l, r);
                if (!std::isfinite (l) || !std::isfinite (r)) ++bad; peak = std::max (peak, (double) std::fabs (l)); }
        }
        printf ("crête max = %.3f, valeurs non finies = %d\n", peak, bad);
    }
    printf (bad ? "ECHEC\n" : "OK\n");
    return bad ? 1 : 0;
}
