#!/usr/bin/env python3
"""
SPSA Tuning Script for Zangdar using fastchess.
Version 1.0

Usage:
    python3 spsa_tune.py [options]

Examples:
    # Tuning standard (200 games/iter, ~500 iterations)
    python3 spsa_tune.py --iterations 500 --games-per-iter 200 --concurrency 7

    # Tuning rapide (moins de parties, plus d'iterations)
    python3 spsa_tune.py --iterations 1000 --games-per-iter 100 --concurrency 7

    # Reprendre un tuning interrompu
    python3 spsa_tune.py --resume

    # TC plus court pour aller plus vite
    python3 spsa_tune.py --tc 5+0.05 --games-per-iter 300 --concurrency 10
"""

import json
import subprocess
import random
import math
import os
import sys
import argparse
import re
import time
import csv
from datetime import datetime, timedelta

__version__ = "1.0"


# ============================================================
#  Groupes de paramètres
# ============================================================

PARAM_GROUPS = {
    "lmr": {
        "desc": "Late Move Reductions",
        "params": [
            "LMR_CaptureBase", "LMR_CaptureDivisor",
            "LMR_QuietBase", "LMR_QuietDivisor",
            "LMR_HistReductionDivisor",
            "LMR_DeeperMargin", "LMR_DeeperScale", "LMR_ShallowerMargin",
        ],
    },
    "pruning": {
        "desc": "Futility, Static Exchange Evaluation, History Pruning",
        "params": [
            "FPMargin", "FPDepth", "FPHistoryLimit", "FPHistoryLimitImproving",
            "SEEPruningDepth", "SEEQuietMargin", "SEENoisyMargin",
            "HistoryPruningDepth", "HistoryPruningScale",
        ],
    },
    "nmp": {
        "desc": "Null Move Pruning, ProbCut, Razoring, Static Null Move Pruning",
        "params": [
            "NMPDepth", "NMPReduction", "NMPMargin", "NMPMax", "NMPDivisor",
            "ProbCutDepth", "ProbCutMargin", "ProbcutReduction",
            "RazoringDepth", "RazoringMargin",
            "SNMPDepth", "SNMPMargin",
        ],
    },
    "history": {
        "desc": "History Bonus & Malus",
        "params": [
        "HistoryBonusScale", "HistoryBonusOffset", "HistoryBonusMax",
        "HistoryMalusScale", "HistoryMalusOffset", "HistoryMalusMax",
        ],
    },
    "misc": {
        "desc": "Aspiration, Delta pruning, Singular Extension",
        "params": [
            "AspirationWindowsDepth", "AspirationWindowsInitial",
            "AspirationWindowsDelta", "AspirationWindowsExpand",
            "DeltaPruningBias",
            "SEDepth",
        ],
    },
    "timer-1": {
        "desc": "Time Management, critique",
        "params": [
            "softTimeScale", "hardTimeScale", "baseTimeScale",
        ],
    },
    "timer-2": {
        "desc": "Time Management, ponderation",
        "params": [
            "incrementScale", "nodeTMBase", "nodeTMScale",
        ],
    },
    "all": {
        "desc": "Tous les paramètres",
        "params": [],  # spécial : prend tout
    },
}


def list_groups():
    """Affiche les groupes disponibles."""
    print("Groupes de paramètres disponibles:\n")
    for name, g in PARAM_GROUPS.items():
        if name == "all":
            print(f"  {name:10s} - {g['desc']}")
        else:
            print(f"  {name:10s} - {g['desc']} ({len(g['params'])} params)")
    print()


# ============================================================
#  Load / Save
# ============================================================

def load_params(config_file):
    """Charge les paramètres tunables depuis config.json."""
    with open(config_file) as f:
        config = json.load(f)

    params = {}
    for name, p in config.items():
        vmin = float(p["min_value"])
        vmax = float(p["max_value"])
        step = float(p["step"])
        param_range = vmax - vmin
        params[name] = {
            "value":   float(p["value"]),
            "initial": float(p["value"]),
            "min":     vmin,
            "max":     vmax,
            "step":    step,
            # Perturbation : max(step, range/20)
            "c": max(step, param_range / 20.0),
            # Facteur d'échelle pour la mise à jour (normalisé par la plage)
            # Premier pas cible : a0 * range_scale / (1+A)^alpha ≈ 2% de la plage
            "range_scale": param_range / 100.0,
        }
    # Validation des paramètres
    warnings = []
    for name, p in params.items():
        val, vmin, vmax, step = p["value"], p["min"], p["max"], p["step"]
        param_range = vmax - vmin

        # Valeur hors bornes
        if val < vmin or val > vmax:
            warnings.append(f"  {name}: value {val:.0f} hors bornes [{vmin:.0f}, {vmax:.0f}]")

        # Grille trop grossière (moins de 5 valeurs possibles)
        if step > 0 and param_range / step < 5:
            n_vals = int(param_range / step) + 1
            warnings.append(f"  {name}: grille trop grossière "
                            f"({n_vals} valeurs possibles, range={param_range:.0f}, step={step:.0f})")

        # Valeur à moins d'un step d'un bord (perturbation asymétrique)
        if step > 0:
            if 0.01 < val - vmin < step:
                warnings.append(f"  {name}: value {val:.0f} à moins d'un step du min "
                                f"(distance={val-vmin:.0f}, step={step:.0f})")
            if 0.01 < vmax - val < step:
                warnings.append(f"  {name}: value {val:.0f} à moins d'un step du max "
                                f"(distance={vmax-val:.0f}, step={step:.0f})")

    if warnings:
        print(f"\n  ATTENTION - problèmes de configuration:")
        for w in warnings:
            print(w)
        print()

    return params


def save_state(filename, iteration, params, history):
    """Sauvegarde l'état complet pour reprendre plus tard."""
    state = {
        "iteration": iteration,
        "timestamp": datetime.now().isoformat(),
        "params": {name: p["value"] for name, p in params.items()},
        "initial": {name: p["initial"] for name, p in params.items()},
        "history": history[-100:],  # garder les 100 dernières itérations
    }
    # Écriture atomique
    tmp = filename + ".tmp"
    with open(tmp, "w") as f:
        json.dump(state, f, indent=2)
    os.replace(tmp, filename)


def save_params_json(filename, params):
    """Sauvegarde les paramètres au format config.json."""
    config = {}
    for name, p in params.items():
        config[name] = {
            "value":     int(round_step(p["value"], p["step"], p["min"])),
            "min_value": int(p["min"]),
            "max_value": int(p["max"]),
            "step":      int(p["step"]),
        }
    with open(filename, "w") as f:
        json.dump(config, f, indent=2)


def save_params_header(filename, params):
    """Sauvegarde les paramètres au format C++ (copier-coller dans Tunable.h)."""
    with open(filename, "w") as f:
        f.write("// Tuned parameters - generated by spsa_tune.py\n")
        f.write(f"// Date: {datetime.now().isoformat()}\n\n")
        for name, p in params.items():
            val = int(round_step(p["value"], p["step"], p["min"]))
            f.write(f"PARAM({name}, {val}, {int(p['min'])}, {int(p['max'])}, {int(p['step'])});\n")


HISTORY_FILE = "spsa_history.csv"
RATIO_FILE   = "spsa_ratio.csv"


def append_history(iteration, params, elo):
    """Ajoute une ligne à l'historique CSV (valeurs des paramètres à chaque itération)."""
    file_exists = os.path.isfile(HISTORY_FILE)
    param_names = sorted(params.keys())

    with open(HISTORY_FILE, "a", newline="") as f:
        writer = csv.writer(f)
        if not file_exists or os.path.getsize(HISTORY_FILE) == 0:
            writer.writerow(["iteration", "elo"] + param_names)
        writer.writerow([iteration, f"{elo:.1f}"] + [params[n]["value"] for n in param_names])

    # Second CSV : ratio valeur_courante / valeur_initiale
    file_exists_r = os.path.isfile(RATIO_FILE)

    ratios = []
    for n in param_names:
        ini = params[n]["initial"]
        if ini == 0:
            eps = 0.01
            ratios.append(f"{(params[n]['value'] + eps) / eps * 100:.2f}")
        else:
            ratios.append(f"{params[n]['value'] / ini * 100:.2f}")
    with open(RATIO_FILE, "a", newline="") as f:
        writer = csv.writer(f)
        if not file_exists_r or os.path.getsize(RATIO_FILE) == 0:
            writer.writerow(["iteration"] + param_names)
        writer.writerow([iteration] + ratios)


def run_analyze():
    """Analyse la convergence à partir de spsa_history.csv et spsa_state.json."""

    if not os.path.isfile(HISTORY_FILE):
        print(f"Erreur: fichier '{HISTORY_FILE}' introuvable.")
        print("Lancez d'abord un tuning pour générer l'historique.")
        sys.exit(1)

    # Lire le CSV
    with open(HISTORY_FILE) as f:
        reader = csv.DictReader(f)
        rows = list(reader)

    if len(rows) < 2:
        print("Pas assez de données pour analyser (minimum 2 itérations).")
        sys.exit(1)

    param_names = [k for k in rows[0].keys() if k not in ("iteration", "elo")]
    n_iter = len(rows)

    # Charger les valeurs initiales depuis spsa_state.json
    initial = {}
    if os.path.isfile("spsa_state.json"):
        with open("spsa_state.json") as f:
            state = json.load(f)
        initial = state.get("initial", {})

    # Charger les steps depuis spsa_params.json
    steps = {}
    for cfg_file in ["spsa_params.json", "config.json"]:
        if os.path.isfile(cfg_file):
            with open(cfg_file) as f:
                cfg = json.load(f)
            steps = {name: float(p.get("step", 1)) for name, p in cfg.items()}
            break

    print(f"\n{'='*70}")
    print(f"  Analyse de convergence SPSA - {n_iter} itérations")
    print(f"{'='*70}\n")

    # ---- Elo moyen par fenêtre ----
    elos = [float(r["elo"]) for r in rows]
    window = min(25, n_iter)
    elo_recent = elos[-window:]
    elo_avg = sum(elos) / len(elos)
    elo_recent_avg = sum(elo_recent) / len(elo_recent)

    print(f"  Elo moyen (toutes itérations):    {elo_avg:+.1f}")
    print(f"  Elo moyen (dernières {window}):       {elo_recent_avg:+.1f}")
    print(f"  (Elo ~ 0 = les perturbations ne font plus de différence = convergence)")
    print()

    # ---- Analyse par paramètre ----
    print(f"  {'Paramètre':30s} {'Initial':>10s} {'Actuel':>10s} {'Delta':>10s} "
          f"{'Tendance':>10s} {'Stabilité':>10s} {'Status':>12s}")
    print(f"  {'-'*30} {'-'*10} {'-'*10} {'-'*10} {'-'*10} {'-'*10} {'-'*12}")

    converged_count = 0

    for name in param_names:
        values = [float(r[name]) for r in rows]

        ini = float(initial.get(name, values[0]))
        current = values[-1]
        delta = current - ini

        # Tendance : régression linéaire simple sur les dernières N itérations
        window_vals = values[-window:]
        n = len(window_vals)
        if n >= 2:
            x_mean = (n - 1) / 2.0
            y_mean = sum(window_vals) / n
            num = sum((i - x_mean) * (v - y_mean) for i, v in enumerate(window_vals))
            den = sum((i - x_mean) ** 2 for i in range(n))
            slope = num / den if den > 0 else 0
        else:
            slope = 0

        # Stabilité : écart-type sur les dernières N itérations
        if n >= 2:
            mean = sum(window_vals) / n
            variance = sum((v - mean) ** 2 for v in window_vals) / (n - 1)
            stddev = math.sqrt(variance)
        else:
            stddev = 0

        # Direction dans les dernières itérations
        if abs(slope) < 0.05:
            trend = "   ~"
        elif slope > 0:
            trend = f"  +{slope:.1f}/it"
        else:
            trend = f"  {slope:.1f}/it"

        # Status de convergence
        step_val = max(1.0, steps.get(name, 1.0))

        if stddev < step_val * 2 and abs(slope) < step_val * 0.1:
            status = "CONVERGE"
            converged_count += 1
        elif abs(slope) < step_val * 0.3:
            status = "~stable"
            converged_count += 0.5
        else:
            status = "en cours"

        print(f"  {name:30s} {ini:10.2f} {current:10.2f} {delta:+10.2f} "
              f"{trend:>10s} {'+-' + f'{stddev:.1f}':>9s} {status:>12s}")

    print()
    pct = converged_count / len(param_names) * 100 if param_names else 0
    print(f"  Convergence globale: {pct:.0f}% ({converged_count:.0f}/{len(param_names)} paramètres)")

    # ---- Evolution Elo ----
    print(f"\n  Evolution Elo (moyennes glissantes sur {window} itérations):")
    step_size = max(1, n_iter // 10)
    for i in range(0, n_iter, step_size):
        chunk = elos[i:i + step_size]
        avg = sum(chunk) / len(chunk) if chunk else 0
        bar_len = int(abs(avg) / 5)
        bar = "+" * bar_len if avg >= 0 else "-" * bar_len
        print(f"    Iter {i+1:4d}-{min(i+step_size, n_iter):4d}: {avg:+6.1f} {bar}")

    # ---- Paramètres les plus modifiés ----
    diffs = []
    for name in param_names:
        values = [float(r[name]) for r in rows]
        ini = float(initial.get(name, values[0]))
        current = values[-1]
        diff = current - ini
        if diff != 0:
            diffs.append((abs(diff), name, diff, current, ini))
    diffs.sort(reverse=True)

    if diffs:
        print(f"\n  Top paramètres modifiés:")
        for _, name, diff, current, ini in diffs[:10]:
            print(f"    {name:30s}: {ini:.2f} -> {current:.2f} ({diff:+.2f})")

    print(f"\n{'='*70}\n")


# ============================================================
#  SPSA
# ============================================================

def clamp(value, lo, hi):
    return max(lo, min(hi, value))


def round_step(value, step, min_val):
    """Arrondi à la grille définie par step, alignée sur min_val."""
    if step <= 0:
        return value
    return min_val + round((value - min_val) / step) * step


def compute_elo(wins, draws, losses):
    """Calcul simplifié de l'Elo logistique."""
    total = wins + draws + losses
    if total == 0:
        return 0.0
    score = (wins + draws * 0.5) / total
    if score <= 0.0:
        return -400.0
    if score >= 1.0:
        return 400.0
    return -400.0 * math.log10(1.0 / score - 1.0)


def elo_error(wins, draws, losses, confidence=0.95):
    """Marge d'erreur Elo (approximation)."""
    total = wins + draws + losses
    if total == 0:
        return 0.0
    score = (wins + draws * 0.5) / total
    w = wins / total
    d = draws / total
    l = losses / total
    var = w * (1 - score)**2 + d * (0.5 - score)**2 + l * (0 - score)**2
    if var <= 0:
        return 0.0
    se = math.sqrt(var / total)
    z = 1.96
    score_lo = max(0.001, min(0.999, score - z * se))
    score_hi = max(0.001, min(0.999, score + z * se))
    elo_lo = -400.0 * math.log10(1.0 / score_lo - 1.0)
    elo_hi = -400.0 * math.log10(1.0 / score_hi - 1.0)
    return (elo_hi - elo_lo) / 2.0


# ============================================================
#  Fastchess
# ============================================================

def run_fastchess(engine, params_plus, params_minus, games, concurrency, tc, hash_mb, book, fastchess_bin):
    """
    Lance un match fastchess entre deux configurations.
    Retourne (wins, draws, losses) du point de vue de params_plus.
    """
    cmd = [fastchess_bin]

    # Engine 1 : perturbation +
    engine_args = ["-engine", f"cmd={engine}", "name=spsa_plus"]
    for name, val in params_plus.items():
        engine_args.append(f"option.{name}={int(round(val))}")
    cmd += engine_args

    # Engine 2 : perturbation -
    engine_args = ["-engine", f"cmd={engine}", "name=spsa_minus"]
    for name, val in params_minus.items():
        engine_args.append(f"option.{name}={int(round(val))}")
    cmd += engine_args

    # Paramètres communs
    rounds = max(1, games // 2)
    cmd += [
        "-each", "proto=uci", f"tc={tc}",
        f"option.Threads=1", f"option.Hash={hash_mb}",
        "-rounds", str(rounds),
        "-games", "2",
        "-repeat",
        "-concurrency", str(concurrency),
        "-openings", f"file={book}", "format=epd", "order=random",
        "-recover",
        "-draw", "movenumber=40", "movecount=8", "score=10",
        "-resign", "movecount=3", "score=400",
    ]

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=7200,  # 2h max
        )
        output = result.stdout + "\n" + result.stderr
    except subprocess.TimeoutExpired:
        print("  TIMEOUT fastchess")
        return 0, 0, 0
    except Exception as e:
        print(f"  ERROR: {e}")
        return 0, 0, 0

    # Parser le résultat (prendre la DERNIÈRE occurrence, pas la première)
    pattern = r"Wins:\s*(\d+),\s*Losses:\s*(\d+),\s*Draws:\s*(\d+)"
    matches = re.findall(pattern, output)

    if not matches:
        pattern2 = r"Score of spsa_plus vs spsa_minus:\s*(\d+)\s*-\s*(\d+)\s*-\s*(\d+)"
        matches = re.findall(pattern2, output)

    if not matches:
        print(f"  WARNING: impossible de parser la sortie fastchess")
        lines = output.strip().split("\n")
        for line in lines[-5:]:
            print(f"    {line}")
        return 0, 0, 0

    last = matches[-1]
    wins   = int(last[0])
    losses = int(last[1])
    draws  = int(last[2])

    return wins, draws, losses


# ============================================================
#  Main
# ============================================================

def main():
    parser = argparse.ArgumentParser(
        description="SPSA Tuner pour Zangdar avec fastchess",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--version", action="version", version=f"%(prog)s {__version__}")
    parser.add_argument("--engine",
        default="./Zangdar",
        help="Chemin vers l'exécutable du moteur (compilé avec -DUSE_TUNING)")
    parser.add_argument("--config",
        default="spsa_params.json",
        help="Fichier de configuration des paramètres (JSON)")
    parser.add_argument("--book",
        default="/mnt/Datas/Echecs/Programmation/Zangdar/tournois personnels/UHO_Lichess_4852_v1.epd",
        help="Livre d'ouvertures (.epd)")
    parser.add_argument("--fastchess",
        default="fastchess",
        help="Chemin vers fastchess")
    parser.add_argument("--tc",
        default="8+0.08",
        help="Contrôle du temps (ex: 8+0.08, 5+0.05)")
    parser.add_argument("--hash",
        type=int, default=64,
        help="Taille table de hachage (MB)")
    parser.add_argument("--concurrency",
        type=int, default=7,
        help="Nombre de parties simultanées")
    parser.add_argument("--games-per-iter",
        type=int, default=200,
        help="Nombre de parties par itération SPSA")
    parser.add_argument("--iterations",
        type=int, default=500,
        help="Nombre total d'itérations SPSA")
    parser.add_argument("--a0",
        type=float, default=3.0,
        help="Multiplicateur du taux d'apprentissage initial. "
             "La mise à jour est normalisée par range/100 pour chaque paramètre, "
             "donc tous les params avancent proportionnellement à leur plage.")
    parser.add_argument("--group",
        default="all",
        help="Groupe de paramètres à tuner (lmr, pruning, nmp, misc, time, all). "
             "Utilisez --list-groups pour voir les détails")
    parser.add_argument("--list-groups",
        action="store_true",
        help="Afficher les groupes de paramètres disponibles")
    parser.add_argument("--analyze",
        action="store_true",
        help="Analyser la convergence du tuning en cours")
    parser.add_argument("--resume",
        action="store_true",
        help="Reprendre depuis la dernière sauvegarde")
    args = parser.parse_args()

    if args.list_groups:
        list_groups()
        sys.exit(0)

    if args.analyze:
        run_analyze()
        sys.exit(0)

    print(f"\n  SPSA Tuner pour Zangdar v{__version__}\n")

    # ----- Vérifications -----
    if not os.path.isfile(args.engine):
        print(f"Erreur: moteur '{args.engine}' introuvable")
        sys.exit(1)

    if not os.path.isfile(args.config):
        print(f"Erreur: fichier config '{args.config}' introuvable")
        print(f"Lancez d'abord le moteur avec 'json' pour générer le fichier")
        sys.exit(1)

    if not os.path.isfile(args.book):
        print(f"Erreur: livre d'ouvertures '{args.book}' introuvable")
        sys.exit(1)

    # Convertir en chemins absolus (nécessaire pour fastchess)
    engine_abs = os.path.abspath(args.engine)
    book_abs   = os.path.abspath(args.book)

    # ----- Paramètres -----
    all_params = load_params(args.config)
    print(f"  {len(all_params)} paramètres tunables chargés depuis {args.config}")

    # Filtrage par groupe
    group_name = args.group.lower()
    if group_name not in PARAM_GROUPS:
        print(f"Erreur: groupe '{args.group}' inconnu")
        list_groups()
        sys.exit(1)

    if group_name == "all":
        params = all_params
    else:
        group_keys = PARAM_GROUPS[group_name]["params"]
        params = {k: v for k, v in all_params.items() if k in group_keys}
        missing = [k for k in group_keys if k not in all_params]
        if missing:
            print(f"  Attention: paramètres absents du config: {', '.join(missing)}")

    print(f"  Groupe '{group_name}': {len(params)} paramètres à tuner "
          f"({PARAM_GROUPS[group_name]['desc']})")

    # ----- Afficher les range_scale pour info -----
    print(f"\n  Facteurs d'échelle par paramètre (range/100):")
    for name, p in params.items():
        print(f"    {name:35s}: range={int(p['max']-p['min']):5d}  scale={p['range_scale']:.2f}  c={p['c']:.1f}")

    # ----- SPSA hyperparamètres -----
    alpha = 0.602
    gamma = 0.101
    A = 0.1 * args.iterations
    a0 = args.a0

    # ----- Reprise / nettoyage -----
    state_file = "spsa_state.json"
    output_files = [HISTORY_FILE, RATIO_FILE, state_file,
                    "spsa_log.txt", "config_tuned.json", "tuned_params.h"]
    history = []
    start_iter = 0
    cumul_w, cumul_d, cumul_l = 0, 0, 0

    if not args.resume:
        for f in output_files:
            if os.path.isfile(f):
                os.remove(f)
                print(f"  Supprimé: {f}")

    if args.resume and os.path.isfile(state_file):
        with open(state_file) as f:
            state = json.load(f)
        start_iter = state["iteration"]
        history = state.get("history", [])
        for name in params:
            if name in state["params"]:
                params[name]["value"] = state["params"][name]
            if name in state.get("initial", {}):
                params[name]["initial"] = state["initial"][name]
        for h in history:
            cumul_w += h.get("wins", 0)
            cumul_d += h.get("draws", 0)
            cumul_l += h.get("losses", 0)
        print(f"  Reprise depuis l'itération {start_iter}")
    else:
        for p in params.values():
            p["initial"] = p["value"]

    # ----- Résumé de la configuration -----
    total_games = args.iterations * args.games_per_iter
    print(f"""
{'='*60}
  SPSA Tuner pour Zangdar v{__version__}
{'='*60}
  Engine:       {args.engine}
  Config:       {args.config}
  Groupe:       {group_name} ({len(params)} params)
  Book:         {os.path.basename(args.book)}
  TC:           {args.tc}
  Hash:         {args.hash} MB
  Concurrency:  {args.concurrency}
  Games/iter:   {args.games_per_iter}
  Iterations:   {args.iterations}
  Total games:  {total_games:,}
  a0:           {a0}  (update = a_k * range_scale * score * delta)
  A:            {A:.0f}
{'='*60}
""")

    # ----- Log -----
    log_file = open("spsa_log.txt", "a")
    log_file.write(f"\n{'='*60}\n")
    log_file.write(f"Session: {datetime.now().isoformat()}\n")
    log_file.write(f"Config: iter={args.iterations}, games={args.games_per_iter}, "
                   f"tc={args.tc}, conc={args.concurrency}, a0={a0}\n")
    log_file.write(f"{'='*60}\n\n")

    session_start = time.time()

    try:
        for k in range(start_iter, args.iterations):
            iter_start = time.time()

            # --- Coefficients SPSA ---
            a_k = a0 / (k + 1 + A) ** alpha
            c_k_scale = 1.0 / (k + 1) ** gamma

            # --- Perturbation ---
            delta = {}
            params_plus = {}
            params_minus = {}

            for name, p in params.items():
                delta[name] = random.choice([-1, 1])
                c = p["c"] * c_k_scale

                val_plus = clamp(
                    round_step(p["value"] + c * delta[name], p["step"], p["min"]),
                    p["min"], p["max"]
                )
                val_minus = clamp(
                    round_step(p["value"] - c * delta[name], p["step"], p["min"]),
                    p["min"], p["max"]
                )

                params_plus[name] = val_plus
                params_minus[name] = val_minus

            # --- Match ---
            elapsed_total = time.time() - session_start
            eta = ""
            if k > start_iter:
                avg_time = elapsed_total / (k - start_iter)
                remaining = avg_time * (args.iterations - k)
                eta = f" | ETA: {timedelta(seconds=int(remaining))}"

            print(f"[{k+1}/{args.iterations}]{eta} ", end="", flush=True)

            wins, draws, losses = run_fastchess(
                engine_abs, params_plus, params_minus,
                args.games_per_iter, args.concurrency,
                args.tc, args.hash, book_abs, args.fastchess
            )

            total = wins + draws + losses
            if total == 0:
                print("SKIP (pas de parties)")
                continue

            # --- Score et Elo ---
            score = (wins - losses) / total   # [-1, 1]
            elo = compute_elo(wins, draws, losses)
            err = elo_error(wins, draws, losses)

            cumul_w += wins
            cumul_d += draws
            cumul_l += losses

            # --- Mise à jour des paramètres ---
            # Normalisation par range_scale : chaque paramètre avance
            # proportionnellement à sa plage (range/100), indépendamment
            # de son échelle absolue. Résout le problème d'hétérogénéité
            # entre params à petite plage (ex: LMR_DeeperScale=10) et
            # grande plage (ex: LMR_HistReductionDivisor=5000).
            for name, p in params.items():
                c = p["c"] * c_k_scale
                if c < 0.001:
                    continue

                update = a_k * p["range_scale"] * score * delta[name]
                new_val = p["value"] + update
                p["value"] = clamp(new_val, p["min"], p["max"])

            elapsed = time.time() - iter_start

            # --- Affichage ---
            result_str = (f"+{wins} ={draws} -{losses}  "
                          f"Elo: {elo:+.1f} +/- {err:.1f}  "
                          f"[{elapsed:.0f}s]")
            print(result_str)

            log_file.write(f"Iter {k+1:4d}: {result_str}\n")
            log_file.flush()

            history.append({
                "iteration": k + 1,
                "wins": wins, "draws": draws, "losses": losses,
                "elo": round(elo, 1),
            })

            # --- Sauvegarde ---
            save_state(state_file, k + 1, params, history)
            append_history(k + 1, params, elo)

            # Sauvegarde périodique des params
            if (k + 1) % 10 == 0:
                save_params_json("config_tuned.json", params)

            # Rapport périodique
            if (k + 1) % 25 == 0:
                print(f"\n  --- Rapport après {k+1} itérations ---")
                print(f"  Cumul: +{cumul_w} ={cumul_d} -{cumul_l} "
                      f"({cumul_w + cumul_d + cumul_l} parties)")
                cumul_elo = compute_elo(cumul_w, cumul_d, cumul_l)
                print(f"  Elo cumulé: {cumul_elo:+.1f}")

                diffs = []
                for name, p in params.items():
                    diff = p["value"] - p["initial"]
                    if diff != 0:
                        diffs.append((abs(diff / max(1, p["step"])), name, diff, p["value"]))
                diffs.sort(reverse=True)
                if diffs:
                    print(f"  Paramètres les plus modifiés:")
                    for _, name, diff, val in diffs[:8]:
                        print(f"    {name:30s}: {val:10.2f}  ({diff:+.2f})")
                print()

    except KeyboardInterrupt:
        print(f"\n\nInterrompu par l'utilisateur (itération {k+1})")

    finally:
        current_iter = k + 1 if "k" in dir() else start_iter
        save_state(state_file, current_iter, params, history)
        save_params_json("config_tuned.json", params)
        save_params_header("tuned_params.h", params)
        log_file.close()

    # --- Résumé final ---
    print(f"\n{'='*60}")
    print(f"  SPSA terminé après {current_iter} itérations")
    print(f"  Parties totales: +{cumul_w} ={cumul_d} -{cumul_l} "
          f"({cumul_w + cumul_d + cumul_l} parties)")
    print(f"{'='*60}")
    print(f"\n  Paramètres finaux:")
    for name, p in params.items():
        val = p["value"]
        ini = p["initial"]
        diff = val - ini
        marker = " *" if diff != 0 else ""
        print(f"    {name:30s}: {val:10.2f}  (initial: {ini:.2f}, delta: {diff:+.2f}){marker}")

    print(f"\nFichiers générés:")
    print(f"  config_tuned.json  - paramètres au format JSON")
    print(f"  tuned_params.h     - paramètres au format C++ (copier dans Tunable.h)")
    print(f"  spsa_state.json    - état complet (pour --resume)")
    print(f"  spsa_log.txt       - historique des itérations")


if __name__ == "__main__":
    main()
