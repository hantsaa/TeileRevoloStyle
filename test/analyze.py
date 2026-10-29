"""Analyse des dumps produits par `test_dsp dump` : niveaux par octave, RT60 par bande, spectre du bruit.
Usage : ./test_dsp dump && python3 test/analyze.py"""
import numpy as np
from scipy.signal import butter, sosfilt

SR = 48000
BANDS = [63, 125, 250, 500, 1000, 2000, 4000, 8000]

def load(n): return np.fromfile(n, dtype=np.float32).astype(np.float64)

def band(x, fc):
    lo, hi = fc / np.sqrt(2), min(fc * np.sqrt(2), SR * 0.45)
    return sosfilt(butter(4, [lo, hi], btype='band', fs=SR, output='sos'), x)

def db(x): return 20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-12)

def rt60(ir, fc):
    y = band(ir, fc); e = np.cumsum((y ** 2)[::-1])[::-1]; e = 10 * np.log10(e / e[0] + 1e-30)
    try:
        i5 = np.argmax(e < -5); i25 = np.argmax(e < -25)
        return 3 * (i25 - i5) / SR   # T20 extrapolé à 60 dB
    except Exception: return float('nan')

print("== RT60 par bande d'octave (réponse impulsionnelle, FILTER = 20 Hz) ==")
print("bande Hz :", *[f"{b:>6}" for b in BANDS])
for name in ("ir_len30.f32", "ir_len72.f32", "ir_len100.f32"):
    ir = load(name)
    print(f"{name:14}", *[f"{rt60(ir, b):6.2f}" for b in BANDS])

for tag in ("pink", "club"):
    x = load(f"{tag}_in.f32")[SR * 4:]
    print(f"\n== {tag} : niveau de la réverb seule vs entrée sèche (dB par octave), REVERB au max ==")
    print("bande Hz :", *[f"{b:>6}" for b in BANDS], "  total")
    for fc in (20, 150):
        y = load(f"{tag}_wet_f{fc}.f32")[SR * 4:]
        d = [db(band(y, b)) - db(band(x, b)) for b in BANDS]
        print(f"FILTER {fc:>3}:", *[f"{v:6.1f}" for v in d], f"  {db(y) - db(x):6.1f}")
    y = load(f"{tag}_wet_f150.f32")[SR * 4:]
    print(f"crête sèche {np.max(np.abs(x)):.2f} / crête réverb {np.max(np.abs(y)):.2f}")

z = load("noise_only.f32")[SR * 2:]
print("\n== Bruit seul (REVERB max, NOISE max, FILTER 20 Hz) : niveau par octave (dBFS) ==")
print("bande Hz :", *[f"{b:>6}" for b in BANDS])
print("niveau   :", *[f"{db(band(z, b)):6.1f}" for b in BANDS], f"  total {db(z):.1f}")

def ripple(ir, t0=0.4, t1=1.4, f0=200, f1=10000):
    """Écart-type (dB) du spectre par rapport à sa version lissée au 1/3 d'octave. ~5,6 dB = champ diffus idéal ;
    nettement plus = résonances / sonorité métallique."""
    seg = ir[int(t0 * SR):int(t1 * SR)]; seg = seg * np.hanning(len(seg))
    P = np.abs(np.fft.rfft(seg)) ** 2; f = np.fft.rfftfreq(len(seg), 1 / SR)
    sel = (f > f0) & (f < f1); fs, Ps = f[sel], P[sel]
    sm = np.array([Ps[(fs > x / 2 ** (1 / 6)) & (fs < x * 2 ** (1 / 6))].mean() for x in fs])
    return np.std(10 * np.log10(Ps / sm))

print("\n== Ondulation du spectre de la queue (≈5,6 dB = diffus idéal ; plus = métallique) ==")
for name in ("ir_len30.f32", "ir_len72.f32", "ir_len100.f32"):
    print(f"{name:14} {ripple(load(name)):5.2f} dB")
