"""analyse_capture.py — turn a recording of the calibration ROM into numbers.

Feed it a WAV of the NGPC headphone output while psg_cal is running. It finds the
segments by their silence gaps, anchors on the double-length MARKER, and reports the
three things hardware can settle that a spec cannot:

  1. the ATTENUATION CURVE  — measured dB per step, against the model's table
  2. the SUMMING LAW        — 2 and 3 channels against the uncorrelated ideal
  3. NOISE vs TONE          — where the noise channel sits at the same attenuation

WHAT THIS CANNOT TELL YOU, AND WHY THAT IS FINE
-----------------------------------------------
It cannot give an ABSOLUTE level: that depends on your recording chain's gain, which
is arbitrary. It does not matter. The tool's absolute output is a headroom choice, not
a fidelity question. Everything above is a RATIO, and ratios survive any gain.

    python analyse_capture.py capture.wav

Uses numpy when present, falls back to the standard library otherwise.
"""
import sys
import wave
import math
import struct

# The model's attenuation table (core/third_party/ngpc_apu/apu_core.hpp, kApuVolumes).
MODEL_VOLUMES = [64, 50, 39, 31, 24, 19, 15, 12, 9, 7, 5, 4, 3, 2, 1, 0]

N_CURVE = 15          # segments 1..15 -> attenuation 0..14
SEG_TONE_S = 1.0
SEG_GAP_S = 0.5


def read_wav_mono(path):
    with wave.open(path, "rb") as w:
        nch, width, rate, nframes = (w.getnchannels(), w.getsampwidth(),
                                     w.getframerate(), w.getnframes())
        raw = w.readframes(nframes)
    if width != 2:
        raise SystemExit(f"WAV 16 bits attendu, trouve {width * 8} bits")
    total = len(raw) // 2
    samples = struct.unpack("<%dh" % total, raw[:total * 2])
    if nch > 1:
        samples = [sum(samples[i:i + nch]) / nch for i in range(0, total - nch + 1, nch)]
    return list(samples), rate


def envelope(samples, rate, win_ms=10.0):
    """Short-time RMS, DC removed. The envelope is what the segmentation runs on."""
    win = max(1, int(rate * win_ms / 1000.0))
    out = []
    for i in range(0, len(samples) - win, win):
        chunk = samples[i:i + win]
        mean = sum(chunk) / win
        acc = 0.0
        for v in chunk:
            d = v - mean
            acc += d * d
        out.append(math.sqrt(acc / win))
    return out, win


def find_loud_segments(env, win, rate):
    """Detect only the CLEARLY audible segments.

    ⚠️ DO NOT SEGMENT THE QUIET STEPS BY THRESHOLD. Attenuation 14 is volume 1 against
    volume 64 at attenuation 0 — about -36 dB — so any threshold loose enough to catch
    it also catches the room tone, and any threshold tight enough to reject the room
    catches nothing below attenuation ~8. The quiet steps are exactly the interesting
    part of the curve, so they are located by TIME instead (see build_grid): the ROM
    plays on a fixed schedule, and a schedule does not care how quiet a segment is."""
    if not env:
        return []
    hi = max(env)
    if hi <= 0:
        return []
    thr = hi * 0.20
    min_len = int(0.25 * rate / win)

    segs = []
    start = None
    for i, e in enumerate(env):
        if e > thr and start is None:
            start = i
        elif e <= thr and start is not None:
            if i - start >= min_len:
                segs.append((start * win, i * win))
            start = None
    if start is not None and len(env) - start >= min_len:
        segs.append((start * win, len(env) * win))
    return segs


def build_grid(marker_start, last_start, rate):
    """The ROM's schedule, fitted to the recording.

    Nominal offsets from the marker's start, in seconds:
        marker  0.0            (2 s long)
        seg k   2.5 + (k-1)*1.5   for k = 1..15   (attenuation 0..14)
        2ch     2.5 + 15*1.5 = 25.0
        3ch     26.5
        noise   28.0
    The console's frame is not exactly 1/60 s, so the whole grid is scaled by the
    measured distance between the two anchors that ARE reliably detectable: the
    marker and the last loud segment (the noise burst)."""
    nominal = [0.0] + [2.5 + i * 1.5 for i in range(N_CURVE)] + [25.0, 26.5, 28.0]
    scale = 1.0
    span = (last_start - marker_start) / float(rate)
    if span > 1.0:
        scale = span / nominal[-1]
    return [marker_start + int(off * scale * rate) for off in nominal], scale


def seg_rms(samples, a, b):
    """RMS of the middle 60 % of a segment: the edges carry the attack and the
    driver's own latch writes, and neither belongs in a level measurement."""
    n = b - a
    a2 = a + int(n * 0.2)
    b2 = b - int(n * 0.2)
    chunk = samples[a2:b2]
    if not chunk:
        return 0.0
    mean = sum(chunk) / len(chunk)
    acc = 0.0
    for v in chunk:
        d = v - mean
        acc += d * d
    return math.sqrt(acc / len(chunk))


def db(x, ref):
    if x <= 0 or ref <= 0:
        return float("-inf")
    return 20.0 * math.log10(x / ref)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    path = sys.argv[1]

    samples, rate = read_wav_mono(path)
    dur = len(samples) / float(rate)
    print(f"capture : {path}")
    print(f"          {dur:.1f} s a {rate} Hz\n")

    env, win = envelope(samples, rate)
    loud = find_loud_segments(env, win, rate)
    print(f"segments francs detectes : {len(loud)}")
    if len(loud) < 4:
        print("!! trop peu de segments audibles. Verifie que l'enregistrement couvre")
        print("   une passe complete (~32 s) et que le gain d'entree n'etait pas nul.")
        return 1

    # The MARKER is twice as long as anything else: that is what anchors a pass.
    lengths = [(b - a) / float(rate) for a, b in loud]
    mi = max(range(len(loud)), key=lambda i: lengths[i])
    marker_start = loud[mi][0]
    print(f"MARKER : {lengths[mi]:.2f} s a t={marker_start / float(rate):.2f} s")

    after = [s for s, _ in loud if s > marker_start]
    if not after:
        print("!! rien apres le marqueur : la passe est incomplete, enregistre plus long")
        return 1
    grid, scale = build_grid(marker_start, after[-1], rate)
    print(f"grille calee sur le marqueur (echelle temporelle {scale:.4f})\n")

    tone_len = int(SEG_TONE_S * scale * rate)
    if grid[-1] + tone_len > len(samples):
        print("!! la passe depasse la fin du fichier ; enregistre une passe complete")
        return 1

    curve = [seg_rms(samples, grid[1 + a], grid[1 + a] + tone_len) for a in range(N_CURVE)]
    sum2 = seg_rms(samples, grid[1 + N_CURVE], grid[1 + N_CURVE] + tone_len)
    sum3 = seg_rms(samples, grid[2 + N_CURVE], grid[2 + N_CURVE] + tone_len)
    noise = seg_rms(samples, grid[3 + N_CURVE], grid[3 + N_CURVE] + tone_len)

    ref = curve[0]
    if ref <= 0:
        print("!! le segment d'attenuation 0 est silencieux : mauvais alignement")
        return 1

    print("=== 1. COURBE D'ATTENUATION ===")
    print(" attn |  mesure dB |  modele dB |  ecart")
    print(" -----+------------+------------+--------")
    worst = 0.0
    for a in range(N_CURVE):
        m = db(curve[a], ref)
        mv = MODEL_VOLUMES[a]
        model = db(mv, MODEL_VOLUMES[0]) if mv > 0 else float("-inf")
        d = m - model
        if abs(d) > abs(worst):
            worst = d
        print(f"  {a:2d}  | {m:9.2f}  | {model:9.2f}  | {d:+6.2f}")
    print(f"\n  ecart maximal au modele : {worst:+.2f} dB")
    if abs(worst) <= 1.5:
        print("  => la table du modele tient. Rien a changer.")
    else:
        print("  => ECART REEL : la table kApuVolumes ne decrit pas ce chip.")
        print("     Reporter les valeurs mesurees dans apu_core.hpp.")

    print("\n=== 2. LOI DE SOMMATION ===")
    ideal2 = 20.0 * math.log10(math.sqrt(2.0))
    ideal3 = 20.0 * math.log10(math.sqrt(3.0))
    m2, m3 = db(sum2, ref), db(sum3, ref)
    print(f"  2 voies : mesure {m2:+.2f} dB | somme lineaire ideale {ideal2:+.2f} dB"
          f" | ecart {m2 - ideal2:+.2f}")
    print(f"  3 voies : mesure {m3:+.2f} dB | somme lineaire ideale {ideal3:+.2f} dB"
          f" | ecart {m3 - ideal3:+.2f}")
    if abs(m2 - ideal2) <= 1.5 and abs(m3 - ideal3) <= 1.5:
        print("  => sommation lineaire, comme le modele. Rien a changer.")
    else:
        print("  => l'etage de sortie COMPRESSE : le modele somme trop franchement.")

    print("\n=== 3. BRUIT vs TON (meme attenuation) ===")
    mn = db(noise, ref)
    print(f"  bruit : {mn:+.2f} dB par rapport a un ton a attenuation 0")
    print("  (le modele leur donne la meme table de volume, donc ~0 dB attendu ;")
    print("   un ecart franc veut dire que le bruit a son propre etage.)")

    print("\nRappel : aucun niveau ABSOLU ici, et c'est normal — il depend du gain")
    print("de ta carte son. Seuls les rapports comptent, et ils sont invariants.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
