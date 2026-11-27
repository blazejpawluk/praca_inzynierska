#!/usr/bin/env python3
"""
Analiza wyników badań do rozdz. 8 – S1, S3, D1, K1, T1 (+ telemetria z logów).

Użycie:
    python analiza_badan.py                 # pliki w bieżącym folderze
    python analiza_badan.py --folder badania/surowe

Wejście (w folderze):
    S1.csv, S3.csv, D1.csv, K1.csv, T1.csv  – logi z konsoli (t_ms;kier;linia)
    Rzeczywiste odległości/kąty w D1 i K1 przyjęte jako równe zadanym (1 m, 2 m, ±90°, ±360°);
    różnice końcowe T1 wpisane w T1_ROZNICE.

Wyjście (folder wyniki/):
    *.csv  – tabele wyników,  *.png / *.pdf – wykresy,  podsumowanie.txt
"""
import argparse
import math
import re
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

# ======================= KONFIGURACJA – UZUPEŁNIJ =======================
PLIKI = {"S1": "S3.csv", "S3": "S3.csv", "D1": "D1.csv", "K1": "K1.csv", "T1": "T1.csv"}

# T1: różnica poza RZECZYWISTA − poza ZGŁOSZONA na końcu przejazdu.
# Klucz = oznaczenie użyte w znaczniku logu (np. "T1 CCW #1 start" -> "CCW"),
# wartości: (dx [cm], dy [cm], dθ [°]). Kierunek jazdy skrypt i tak ustala z trasy.
T1_ROZNICE = {
    "CCW": (0.0, -18.5, -13.6),
    "CW": (0.5, 38.0, 1.0),
}
TEMP_C = 21.0      # [°C] temperatura podczas S1 (korekta prędkości dźwięku)

S1_POMIN_MS = 500  # [ms] pomiń początek każdego punktu (przestawianie płyty)
S3_ODL_MM = 1000   # [mm] odległość kija w S3
S3_TOL_MM = 50     # [mm] tolerancja uznania pomiaru za wykrycie kija
S3_PROG = 2 / 3    # odsetek przejść z wykryciem, by kąt należał do wiązki

POZA_PO_MS = 1000  # [ms] poza zgłoszona = pierwsze A:POS po tylu ms od "koniec"
T1_BOK_MM = 1000   # [mm] bok kwadratu w T1
# ========================================================================

OUT = Path("wyniki")
RAPORT = []


def raport(txt=""):
    print(txt)
    RAPORT.append(txt)


def zapisz_wykres(fig, nazwa):
    fig.tight_layout()
    for ext in ("png", "pdf"):
        fig.savefig(OUT / f"{nazwa}.{ext}", dpi=200)
    plt.close(fig)


def wrap(a):
    """Kąt sprowadzony do przedziału (-180, 180]."""
    return -((-np.asarray(a, dtype=float) + 180) % 360 - 180)


# ----------------------------- wczytywanie ------------------------------
def wczytaj_log(sciezka):
    for enc in ("utf-8-sig", "cp1250"):
        try:
            df = pd.read_csv(sciezka, sep=";", encoding=enc, dtype=str,
                             on_bad_lines="skip", engine="python")
            break
        except UnicodeDecodeError:
            continue
    df.columns = [c.strip() for c in df.columns]
    df["t_ms"] = pd.to_numeric(df["t_ms"], errors="coerce")
    df["kier"] = df["kier"].str.strip()
    df["linia"] = df["linia"].astype(str).str.strip()
    return df.dropna(subset=["t_ms"]).sort_values("t_ms").reset_index(drop=True)


def wiadomosci(df, typ, kolumny):
    w = df[(df.kier == "RX") & df.linia.str.startswith(f"A:{typ}:")]
    if w.empty:
        return pd.DataFrame(columns=["t_ms"] + kolumny, dtype=float)
    wart = w.linia.str.split(":", n=2).str[2].str.split(",", expand=True)
    wart = wart.iloc[:, : len(kolumny)].apply(pd.to_numeric, errors="coerce")
    wart.columns = kolumny[: wart.shape[1]]
    out = pd.concat([w[["t_ms"]].reset_index(drop=True), wart.reset_index(drop=True)], axis=1)
    return out.dropna().reset_index(drop=True)


def pozy(df):
    p = wiadomosci(df, "POS", ["x", "y", "th", "v"])
    p["th"] = p["th"] / 100.0
    return p


def znaczniki(df):
    return df[df.kier == "MARK"][["t_ms", "linia"]].reset_index(drop=True)


def przejazdy(df, test):
    """Lista przejazdów: (etykieta startu, t_start, t_koniec) wg znaczników."""
    m = znaczniki(df)
    wynik, start = [], None
    for _, r in m.iterrows():
        txt = r.linia.lower()
        if "koniec" in txt:
            if start is None:
                raport(f"  [{test}] UWAGA: 'koniec' bez znacznika startu (t={r.t_ms:.0f})")
                continue
            wynik.append((start[0], start[1], r.t_ms))
            start = None
        elif "róg" in txt or "rog" in txt:
            continue
        else:
            start = (r.linia, r.t_ms)
    return wynik


def poza_po(pos, t):
    p = pos[pos.t_ms > t + POZA_PO_MS]
    return None if p.empty else p.iloc[0]


# --------------------------------- S1 -----------------------------------
def analiza_S1(df):
    raport("\n=== S1 – dokładność odległości sonaru ===")
    dist = wiadomosci(df, "DIST", ["k", "d", "x", "y", "th"])
    m = znaczniki(df)
    t_kolejny = list(m.t_ms[1:]) + [np.inf]
    v_T = 331.3 + 0.606 * TEMP_C
    wiersze = []
    for (_, r), t1 in zip(m.iterrows(), t_kolejny):
        mt = re.search(r"d\s*=\s*([\d.,]+)", r.linia)
        if not mt or "s1" not in r.linia.lower():
            continue
        d_rz = float(mt.group(1).replace(",", ".")) * 10  # cm -> mm
        s = dist[(dist.t_ms > r.t_ms + S1_POMIN_MS) & (dist.t_ms < t1)].d
        echo = s[s > 0]
        sr = echo.mean() if len(echo) else np.nan
        wiersze.append({
            "d_rz_cm": d_rz / 10, "n": len(s),
            "bez_echa_%": 100 * (s == 0).mean() if len(s) else np.nan,
            "d_zm_cm": sr / 10, "sigma_cm": echo.std() / 10,
            "bias_cm": (sr - d_rz) / 10,
            "max_blad_cm": (echo - d_rz).abs().max() / 10 if len(echo) else np.nan,
            "d_kor_cm": sr * v_T / 343 / 10,
            "bias_kor_cm": (sr * v_T / 343 - d_rz) / 10,
        })
    t = pd.DataFrame(wiersze).sort_values("d_rz_cm")
    if t.empty:
        raport("  Nie znaleziono znaczników 'S1 d=...'.")
        return
    t.round(2).to_csv(OUT / "S1_tabela.csv", sep=";", index=False, decimal=",")
    raport(t.round(2).to_string(index=False))

    ok = t.dropna(subset=["d_zm_cm"])
    a, b = np.polyfit(ok.d_rz_cm, ok.d_zm_cm, 1)
    r2 = np.corrcoef(ok.d_rz_cm, ok.d_zm_cm)[0, 1] ** 2
    ak, bk = np.polyfit(ok.d_rz_cm, ok.d_kor_cm, 1)
    raport(f"Regresja: d_zm = {a:.4f}·d_rz + {b:.2f} cm (R² = {r2:.5f})")
    raport(f"Po korekcie temp. ({TEMP_C} °C, v = {v_T:.1f} m/s): d = {ak:.4f}·d_rz + {bk:.2f} cm")
    raport(f"Średni |bias| = {ok.bias_cm.abs().mean():.2f} cm, maks. σ = {ok.sigma_cm.max():.2f} cm")

    fig, ax = plt.subplots(1, 2, figsize=(10, 4))
    ax[0].errorbar(ok.d_rz_cm, ok.d_zm_cm, yerr=ok.sigma_cm, fmt="o", capsize=3, label="pomiar ± σ")
    xs = np.linspace(0, ok.d_rz_cm.max() * 1.05, 10)
    ax[0].plot(xs, xs, "k--", lw=1, label="y = x")
    ax[0].set(xlabel="odległość rzeczywista [cm]", ylabel="odległość zmierzona [cm]")
    ax[0].legend(); ax[0].grid(alpha=.3)
    ax[1].plot(ok.d_rz_cm, ok.bias_cm, "o-", label="bias")
    ax[1].plot(ok.d_rz_cm, ok.bias_kor_cm, "s--", label=f"bias po korekcie ({TEMP_C} °C)")
    ax[1].axhline(0, color="k", lw=.8)
    ax[1].set(xlabel="odległość rzeczywista [cm]", ylabel="błąd [cm]")
    ax[1].legend(); ax[1].grid(alpha=.3)
    zapisz_wykres(fig, "S1_wykres")


# --------------------------------- S3 -----------------------------------
def analiza_S3(df):
    raport("\n=== S3 – szerokość wiązki ===")
    dist = wiadomosci(df, "DIST", ["k", "d", "x", "y", "th"])
    m = znaczniki(df)
    m3 = m[m.linia.str.lower().str.contains("s3")]
    if not m3.empty:
        t0 = m3.t_ms.iloc[0]
        dalej = m[m.t_ms > t0]
        t1 = dalej.t_ms.iloc[0] if not dalej.empty else np.inf
        dist = dist[(dist.t_ms > t0) & (dist.t_ms < t1)]
    if dist.empty:
        raport("  Brak pomiarów A:DIST.")
        return
    dist = dist.assign(wykr=(dist.d - S3_ODL_MM).abs() < S3_TOL_MM)
    g = dist.groupby("k").agg(n=("d", "size"), wykrycia=("wykr", "sum"),
                              echo_proc=("d", lambda s: 100 * (s > 0).mean()))
    g["wykrycie_%"] = 100 * g.wykrycia / g.n
    g.round(1).to_csv(OUT / "S3_tabela.csv", sep=";", decimal=",")
    katy = g.index.values
    krok = np.median(np.diff(katy)) if len(katy) > 1 else 2
    w = g[g["wykrycie_%"] >= 100 * S3_PROG].index.values
    if len(w) == 0:
        raport("  Kij nie został wykryty – sprawdź S3_ODL_MM / S3_TOL_MM.")
    else:
        # najdłuższy ciągły przedział zawierający kąt najbliższy 90°
        grupy = np.split(w, np.where(np.diff(w) > krok * 1.5)[0] + 1)
        grupa = min(grupy, key=lambda a: np.min(np.abs(a - 90)))
        szer = grupa.max() - grupa.min()
        raport(f"Przejść na kąt (mediana): {int(g.n.median())}, krok serwa: {krok:g}°")
        raport(f"Wykrycie w ≥{100*S3_PROG:.0f}% przejść: {grupa.min():g}°–{grupa.max():g}° "
               f"→ szerokość wiązki {szer:g}° (do {szer + krok:g}° z uwzgl. kroku); "
               f"środek {(grupa.min()+grupa.max())/2:g}° (oczekiwane 90°)")
        raport(f"Specyfikacja: 15° (α = 7,5°); zmierzone α ≈ {szer/2:.1f}°")
    fig, ax = plt.subplots(figsize=(7, 3.5))
    ax.bar(g.index, g["wykrycie_%"], width=krok * .8)
    ax.axhline(100 * S3_PROG, color="r", ls="--", lw=1, label="próg")
    ax.axvline(90, color="k", lw=.8)
    ax.set(xlabel="kąt serwomechanizmu [°]", ylabel="wykrycie kija [%]",
           title=f"Kij w odległości {S3_ODL_MM/10:.0f} cm")
    ax.legend(); ax.grid(alpha=.3)
    zapisz_wykres(fig, "S3_wykres")


# --------------------------------- D1 -----------------------------------
def analiza_D1(df):
    raport("\n=== D1 – przejechana odległość (rzeczywista = zadana) ===")
    pos = pozy(df)
    wiersze = []
    for i, (etyk, t0, t1) in enumerate(przejazdy(df, "D1")):
        p = poza_po(pos, t1)
        mt = re.search(r"([\d.,]+)\s*m\b", etyk)
        if p is None or not mt:
            raport(f"  pomijam '{etyk}' (brak pozy lub odległości w nazwie)")
            continue
        s_rz = float(mt.group(1).replace(",", ".")) * 1000
        s_od = math.hypot(p.x, p.y)
        wiersze.append({"nr": i + 1, "etykieta": etyk, "s_rz_mm": s_rz,
                        "x_od": p.x, "y_od": p.y, "th_od": p.th, "s_od_mm": s_od,
                        "blad_mm": s_od - s_rz, "blad_proc": 100 * (s_od - s_rz) / s_rz,
                        "k": s_rz / s_od})
    t = pd.DataFrame(wiersze)
    if t.empty:
        raport("  Nie znaleziono przejazdów.")
        return
    t.round(3).to_csv(OUT / "D1_przejazdy.csv", sep=";", index=False, decimal=",")
    raport(t.round(2).to_string(index=False))
    g = t.groupby("s_rz_mm").agg(n=("k", "size"), s_od_sr=("s_od_mm", "mean"),
                                 s_od_sigma=("s_od_mm", "std"), blad_sr_mm=("blad_mm", "mean"),
                                 blad_proc_sr=("blad_proc", "mean"), blad_proc_sigma=("blad_proc", "std"),
                                 blad_max_mm=("blad_mm", lambda s: s.abs().max()),
                                 y_od_sr=("y_od", "mean"), th_od_sr=("th_od", "mean"),
                                 th_od_sigma=("th_od", "std"))
    g.round(3).to_csv(OUT / "D1_podsumowanie.csv", sep=";", decimal=",")
    raport("\n" + g.round(2).to_string())
    k = t.k.mean()
    raport(f"Średni współczynnik skali k = {k:.4f} (σ = {t.k.std():.4f}) "
           f"→ efektywna średnica koła D_ef = {65*k:.2f} mm (przyjęto 65 mm)")
    raport("y_od i th_od to boczne zejście i obrót, które zarejestrowała sama odometria "
           "(rzeczywistego zejścia nie mierzono).")
    fig, ax = plt.subplots(1, 2, figsize=(10, 4))
    for s_rz, gr in t.groupby("s_rz_mm"):
        ax[0].scatter(np.full(len(gr), s_rz / 10), gr.blad_mm / 10, label=f"{s_rz/1000:g} m")
    ax[0].axhline(0, color="k", lw=.8)
    ax[0].set(xlabel="odległość zadana [cm]", ylabel="błąd odometrii [cm]")
    ax[0].legend(); ax[0].grid(alpha=.3)
    for s_rz, gr in t.groupby("s_rz_mm"):
        ax[1].scatter(gr.x_od / 10, gr.y_od / 10, label=f"{s_rz/1000:g} m")
    ax[1].set(xlabel="x zgłoszone [cm]", ylabel="y zgłoszone [cm]",
              title="Położenie końcowe wg odometrii")
    ax[1].legend(); ax[1].grid(alpha=.3)
    zapisz_wykres(fig, "D1_wykres")


# --------------------------------- K1 -----------------------------------
def analiza_K1(df):
    raport("\n=== K1 – obrót w miejscu (rzeczywisty = zadany) ===")
    pos = pozy(df)
    wiersze = []
    for i, (etyk, t0, t1) in enumerate(przejazdy(df, "K1")):
        p = poza_po(pos, t1)
        mt = re.search(r"K1\s*([+-]?\s*\d+)", etyk, re.I)
        if p is None or not mt:
            raport(f"  pomijam '{etyk}' (brak pozy lub kąta w nazwie)")
            continue
        cel = float(mt.group(1).replace(" ", ""))
        obrot_od = cel + float(wrap(p.th - cel))   # wielokrotność 360° wg kąta zadanego
        wiersze.append({"nr": i + 1, "etykieta": etyk, "cel": cel, "th_od": p.th,
                        "obrot_od": obrot_od, "e_th": obrot_od - cel,
                        "skala": cel / obrot_od})
    t = pd.DataFrame(wiersze)
    if t.empty:
        raport("  Nie znaleziono obrotów.")
        return
    t.round(3).to_csv(OUT / "K1_obroty.csv", sep=";", index=False, decimal=",")
    raport(t.round(2).to_string(index=False))
    g = t.groupby("cel").agg(n=("e_th", "size"), e_sr=("e_th", "mean"), e_sigma=("e_th", "std"),
                             e_max=("e_th", lambda s: s.abs().max()), skala_sr=("skala", "mean"))
    g["e_proc"] = 100 * g.e_sr / g.index.to_series().abs()
    g.round(3).to_csv(OUT / "K1_podsumowanie.csv", sep=";", decimal=",")
    raport("\n" + g.round(3).to_string())
    raport(f"Średnia skala (obrót zadany / zgłoszony) = {t.skala.mean():.4f}")
    fig, ax = plt.subplots(figsize=(6, 4))
    ax.scatter(t.cel, t.e_th)
    ax.axhline(0, color="k", lw=.8)
    ax.set(xlabel="obrót zadany [°]", ylabel="błąd: obrót zgłoszony − zadany [°]")
    ax.grid(alpha=.3)
    zapisz_wykres(fig, "K1_wykres")


# --------------------------------- T1 -----------------------------------
def analiza_T1(df):
    raport("\n=== T1 – kwadrat 1×1 m (UMBmark) ===")
    pos = pozy(df)
    wiersze, trasy = [], []
    for i, (etyk, t0, t1) in enumerate(przejazdy(df, "T1")):
        p = poza_po(pos, t1)
        seg = pos[(pos.t_ms >= t0) & (pos.t_ms <= t1)]
        if p is None or seg.empty:
            continue
        if seg.y.min() < -T1_BOK_MM / 3:
            kier = "CW"
        elif seg.y.max() > T1_BOK_MM / 3:
            kier = "CCW"
        else:
            kier = "?"
        etyk_kier = "CCW" if "ccw" in etyk.lower() else ("CW" if "cw" in etyk.lower() else "?")
        if etyk_kier != kier:
            raport(f"  przejazd {i+1} '{etyk}': oznaczony {etyk_kier}, z trasy wynika {kier} – używam {kier}")
        w = {"nr": i + 1, "etykieta": etyk, "kierunek": kier,
             "x_od": p.x, "y_od": p.y, "th_od": p.th,
             "droga_od_mm": np.hypot(np.diff(seg.x), np.diff(seg.y)).sum()}
        if etyk_kier in T1_ROZNICE:
            dx, dy, dth = T1_ROZNICE[etyk_kier]
            w.update(eps_x=dx * 10, eps_y=dy * 10, eps_th=dth,
                     eps_r=math.hypot(dx, dy) * 10,
                     x_rz=p.x + dx * 10, y_rz=p.y + dy * 10)
        else:
            raport(f"  brak różnicy w T1_ROZNICE dla oznaczenia '{etyk_kier}'")
        wiersze.append(w)
        trasy.append((kier, seg, w))
    t = pd.DataFrame(wiersze)
    if t.empty:
        raport("  Nie znaleziono przejazdów.")
        return
    t.round(2).to_csv(OUT / "T1_przejazdy.csv", sep=";", index=False, decimal=",")
    raport(t.round(1).to_string(index=False))
    if "eps_x" not in t:
        return
    t = t.dropna(subset=["eps_x"])
    r = {}
    for kier in ("CW", "CCW"):
        g = t[t.kierunek == kier]
        if g.empty:
            continue
        xc, yc = g.eps_x.mean(), g.eps_y.mean()
        r[kier] = math.hypot(xc, yc)
        raport(f"{kier}: n={len(g)}, środek ciężkości ({xc/10:.1f}; {yc/10:.1f}) cm, "
               f"r = {r[kier]/10:.1f} cm, ε_θ = {g.eps_th.mean():.1f}°")
    if r:
        emax = max(r.values())
        droga = 4 * T1_BOK_MM
        raport(f"E_max,syst = {emax/10:.1f} cm = {100*emax/droga:.2f}% drogi {droga/1000:.0f} m "
               f"(po 1 przejeździe na kierunek – wynik orientacyjny)")
        raport(f"Średni |ε_θ| = {t.eps_th.abs().mean():.1f}° na pętlę 4×90°")

    fig, ax = plt.subplots(1, 2, figsize=(11, 5))
    kol = {"CW": "tab:blue", "CCW": "tab:orange", "?": "gray"}
    for kier, seg, w in trasy:
        ax[0].plot(seg.x / 10, seg.y / 10, color=kol[kier], lw=1, label=f"{kier} – odometria")
        if "x_rz" in w:
            ax[0].plot(w["x_rz"] / 10, w["y_rz"] / 10, "x", color=kol[kier], ms=10,
                       mew=2, label=f"{kier} – koniec rzeczywisty")
    b = T1_BOK_MM / 10
    for s in (1, -1):
        ax[0].plot([0, b, b, 0, 0], [0, 0, s * b, s * b, 0], "k:", lw=.8)
    ax[0].set(xlabel="x [cm]", ylabel="y [cm]", aspect="equal", title="Trasy")
    ax[0].legend(fontsize=8); ax[0].grid(alpha=.3)
    for kier in ("CW", "CCW"):
        g = t[t.kierunek == kier]
        if not g.empty:
            ax[1].scatter(g.eps_x / 10, g.eps_y / 10, color=kol[kier], s=60, label=kier)
            for _, rr in g.iterrows():
                ax[1].annotate(f"ε_θ = {rr.eps_th:+.1f}°", (rr.eps_x / 10, rr.eps_y / 10),
                               textcoords="offset points", xytext=(8, 5), fontsize=8)
    lim = 1.3 * max(t.eps_x.abs().max(), t.eps_y.abs().max(), 10) / 10
    ax[1].set_xlim(-lim, lim); ax[1].set_ylim(-lim, lim)
    ax[1].axhline(0, color="k", lw=.8); ax[1].axvline(0, color="k", lw=.8)
    ax[1].set(xlabel="ε_x [cm]", ylabel="ε_y [cm]", aspect="equal",
              title="Błąd końcowy (rzeczywista − odometria)")
    ax[1].legend(); ax[1].grid(alpha=.3)
    zapisz_wykres(fig, "T1_wykres")


# ----------------------------- telemetria -------------------------------
def analiza_telemetrii(logi):
    raport("\n=== Telemetria – odstępy A:POS (dane do 8.4) ===")
    wiersze = []
    for nazwa, df in logi.items():
        p = pozy(df)
        if len(p) < 10:
            continue
        dt = p.t_ms.diff().dropna()
        brak = ((dt / 200).round() - 1).clip(lower=0).sum()
        wiersze.append({"log": nazwa, "n": len(p), "dt_sr_ms": dt.mean(), "dt_sigma_ms": dt.std(),
                        "dt_p99_ms": dt.quantile(.99), "dt_max_ms": dt.max(),
                        "zgubione_%": 100 * brak / (len(dt) + brak)})
    if wiersze:
        t = pd.DataFrame(wiersze)
        t.round(2).to_csv(OUT / "telemetria.csv", sep=";", index=False, decimal=",")
        raport(t.round(2).to_string(index=False))
        raport("Uwaga: przerwy dłuższe niż kilka sekund (np. między testami) zawyżają dt_max i 'zgubione'.")


# --------------------------------- main ---------------------------------
def main():
    global OUT
    ap = argparse.ArgumentParser()
    ap.add_argument("--folder", default=".", help="folder z plikami CSV")
    args = ap.parse_args()
    folder = Path(args.folder)
    OUT = folder / "wyniki"
    OUT.mkdir(exist_ok=True)

    logi = {}
    for test, plik in PLIKI.items():
        p = folder / plik
        if p.exists():
            logi[test] = wczytaj_log(p)
            m = znaczniki(logi[test])
            raport(f"{plik}: {len(logi[test])} wierszy, {len(m)} znaczników")
        else:
            raport(f"{plik}: brak – pomijam")

    if "S1" in logi: analiza_S1(logi["S1"])
    if "S3" in logi: analiza_S3(logi["S3"])
    if "D1" in logi: analiza_D1(logi["D1"])
    if "K1" in logi: analiza_K1(logi["K1"])
    if "T1" in logi: analiza_T1(logi["T1"])
    analiza_telemetrii(logi)

    (OUT / "podsumowanie.txt").write_text("\n".join(RAPORT), encoding="utf-8")
    print(f"\nWyniki zapisano w: {OUT.resolve()}")


if __name__ == "__main__":
    main()
