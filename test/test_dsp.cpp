// Tests du moteur audio. Sans argument : vérifications de base (stabilité, porte MOM...).
// Avec l'argument "dump" : écrit des fichiers .f32 analysés par test/analyze.py (spectre, RT60 par bande).
#include "../Source/RevoloDSP.h"
#include <cstdio>
#include <random>
#include <string>
using namespace revolo;

static const double SR = 48000.0;

static double rms (const std::vector<float>& v, size_t a, size_t b)
{ double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt (s / (double) (b - a)); }

static void save (const std::string& name, const std::vector<float>& v)
{ FILE* f = fopen (name.c_str(), "wb"); fwrite (v.data(), sizeof (float), v.size(), f); fclose (f); }

static std::vector<float> pinkNoise (size_t n, float amp, unsigned seed)
{
    std::mt19937 rng (seed); std::normal_distribution<float> nd (0.f, 1.f);
    std::vector<float> o (n); float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    for (auto& s : o)
    {
        const float w = nd (rng);
        b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f; b2 = 0.96900f * b2 + w * 0.1538520f;
        b3 = 0.86650f * b3 + w * 0.3104856f; b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
        s = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f) * 0.11f * amp; b6 = w * 0.115926f;
    }
    return o;
}

// « Boucle de club » : kick 52 Hz 4/4 à 124 BPM + basse continue 46 Hz + grosse caisse de bruit rose
static std::vector<float> clubLoop (size_t n)
{
    std::vector<float> o (n, 0.f);
    const double beat = 60.0 / 124.0 * SR;
    for (size_t i = 0; i < n; ++i)
    {
        const double t = std::fmod ((double) i, beat) / SR;
        const double kick = 0.9 * std::sin (2 * M_PI * (52.0 + 90.0 * std::exp (-t * 40.0)) * t) * std::exp (-t * 9.0);
        const double bass = 0.35 * std::sin (2 * M_PI * 46.0 * (double) i / SR);
        o[i] = (float) (kick + bass);
    }
    auto p = pinkNoise (n, 0.12f, 7);
    for (size_t i = 0; i < n; ++i) o[i] += p[i];
    return o;
}

static std::vector<float> run (const std::vector<float>& in, Params p, bool right = false)
{
    Engine e; e.prepare (SR); e.setParams (p, true);
    std::vector<float> out (in.size());
    for (size_t i = 0; i < in.size(); ++i) { float l, r; e.processSample (in[i], in[i] * 0.93f, l, r); out[i] = right ? r : l; }
    return out;
}

int main (int argc, char** argv)
{
    if (argc > 1 && std::string (argv[1]) == "dump")
    {
        const size_t n = (size_t) (SR * 12);
        // réponse impulsionnelle (réverb seule, WET), plusieurs durées et FILTER au minimum (pire cas pour le grave)
        for (float len : { 0.3f, 0.72f, 1.0f })
        {
            Params p; p.length = len; p.filterHz = 20.f; p.air = 0.5f;
            std::vector<float> imp (n, 0.f); imp[0] = 1.f;
            save ("ir_len" + std::to_string ((int) (len * 100)) + ".f32", run (imp, p));
        }
        // réponse à du signal « musical » : tout au max, FILTER à 20 Hz (aucun filtrage utilisateur) puis à 150 Hz
        auto pink = pinkNoise (n, 0.35f, 1); auto club = clubLoop (n);
        for (float fc : { 20.f, 150.f })
        {
            Params p; p.filterHz = fc; p.mixMode = false;
            save ("pink_wet_f" + std::to_string ((int) fc) + ".f32", run (pink, p));
            save ("club_wet_f" + std::to_string ((int) fc) + ".f32", run (club, p));
        }
        save ("pink_in.f32", pink); save ("club_in.f32", club);
        // bruit seul (REVERB au max, aucune entrée) : pour vérifier l'absence de grave
        { Params p; p.noise = 1.f; p.filterHz = 20.f; std::vector<float> z (n, 0.f);
          save ("noise_only.f32", run (z, p)); }
        printf ("dump ok\n"); return 0;
    }

    std::mt19937 rng (1); std::normal_distribution<float> nd (0.f, 0.1f);
    int bad = 0;

    printf ("== Porte MOM : active=false => rien en sortie WET ; la queue décroît après relâchement ==\n");
    {
        Engine e; e.prepare (SR); Params p; p.active = false; e.setParams (p, true);
        std::vector<float> o; for (int i = 0; i < 48000; ++i) { float l, r; e.processSample (nd (rng), nd (rng), l, r); o.push_back (l); }
        const double closed = rms (o, 24000, 48000);
        printf ("porte fermée : RMS sortie = %.6f (doit être ~0)\n", closed); if (closed > 1e-6) ++bad;
        p.active = true; e.setParams (p); for (int i = 0; i < 24000; ++i) { float l, r; e.processSample (nd (rng), nd (rng), l, r); }
        p.active = false; e.setParams (p); o.clear();
        for (int i = 0; i < 96000; ++i) { float l, r; e.processSample (nd (rng), nd (rng), l, r); o.push_back (l); }
        const double a = rms (o, 0, 24000), b = rms (o, 72000, 96000);
        printf ("après relâchement : RMS 0-0.5s = %.4f, 1.5-2s = %.4f\n", a, b); if (!(b < a * 0.5)) ++bad;
    }

    printf ("== Stabilité : balayage rapide des réglages, bruit au max, entrée forte ==\n");
    {
        Engine e; e.prepare (SR); Params p; p.noise = 1.f; p.tremolo = 1.f; p.mixMode = true; double peak = 0;
        for (int blk = 0; blk < 4000; ++blk)
        {
            p.character = (float) (blk % 37) / 36.f; p.length = (float) (blk % 23) / 22.f; p.air = (float) (blk % 11) / 10.f;
            p.filterHz = 20.f * std::pow (1000.f, (float) (blk % 29) / 28.f); p.tempoHz = 0.2f + (float) (blk % 13) * 3.f;
            p.active = (blk / 50) % 2 == 0; e.setParams (p);
            for (int i = 0; i < 64; ++i) { float l, r; e.processSample (nd (rng) * 6.f, nd (rng) * 6.f, l, r);
                if (!std::isfinite (l) || !std::isfinite (r)) ++bad; peak = std::max (peak, (double) std::fabs (l - (p.mixMode ? 0.f : 0.f))); }
        }
        printf ("valeurs non finies = %d\n", bad);
    }

    printf ("== La réverb seule ne dépasse jamais 1.0 (limiteur doux), même avec une entrée saturée ==\n");
    {
        Engine e; e.prepare (SR); Params p; p.mixMode = false; e.setParams (p, true); double peak = 0;
        for (int i = 0; i < 48000 * 4; ++i) { float l, r; e.processSample ((i / 2000) % 2 ? 1.f : -1.f, 1.f, l, r); peak = std::max (peak, (double) std::fabs (l)); }
        printf ("crête réverb seule = %.3f\n", peak); if (peak > 1.0001) ++bad;
    }
    printf (bad ? "ECHEC\n" : "OK\n");
    return bad ? 1 : 0;
}
