#!/usr/bin/env python3
"""
Run N battles between two bot executables using the Troll Farm referee.

Usage:
    python3 battle.py <bot1> <bot2> [-n N] [-l league] [-j jobs] [-s seed]

Outputs:
    runs/<timestamp>/
        summary.txt          overall win-rate table
        summary.json         machine-readable results
        game_<seed>.log      combined stdout+stderr for each game
"""

import argparse
import subprocess
import os
import random
import datetime
import json
from concurrent.futures import ThreadPoolExecutor, as_completed

JAR = os.path.join(os.path.dirname(__file__),
                   "../Troll-Farm/target/troll-farm-1.0-SNAPSHOT.jar")
JAR = os.path.abspath(JAR)

RUNS_DIR = os.path.join(os.path.dirname(__file__), "runs")


def run_game(bot1: str, bot2: str, league: int, seed: int, log_dir: str):
    json_path = os.path.join(log_dir, f"game_{seed}.json")
    cmd = [
        "java", "-jar", JAR,
        "-p1", bot1,
        "-p2", bot2,
        "-league", str(league),
        "-seed", str(seed),
        "-l", json_path,
    ]
    try:
        proc = subprocess.run(
            cmd, capture_output=True, text=True, timeout=30
        )
    except subprocess.TimeoutExpired:
        return seed, None, None, "TIMEOUT"

    # Parse scores from stdout
    score1 = score2 = None
    for line in proc.stdout.splitlines():
        if line.startswith("WARNING") or line.startswith("seed="):
            continue
        try:
            val = int(line.strip())
            if score1 is None:
                score1 = val
            elif score2 is None:
                score2 = val
        except ValueError:
            pass

    if score1 is None or score2 is None:
        return seed, None, None, "PARSE_ERROR"

    winner = "P1" if score1 > score2 else ("P2" if score2 > score1 else "DRAW")

    # Extract per-player logs from JSON (-l output)
    try:
        with open(json_path) as f:
            data = json.load(f)
        errors  = data.get("errors",  {})   # per-turn stderr per player
        outputs = data.get("outputs", {})   # per-turn stdout per player

        for pidx, label in (("0", "p1"), ("1", "p2")):
            log_path = os.path.join(log_dir, f"game_{seed}_{label}.log")
            with open(log_path, "w") as f:
                f.write(f"seed={seed}  score1={score1}  score2={score2}  winner={winner}\n")
                f.write("=" * 60 + "\n")
                turns_err = errors.get(pidx, [])
                turns_out = outputs.get(pidx, [])
                n_turns = max(len(turns_err), len(turns_out))
                for turn in range(n_turns):
                    err = turns_err[turn] if turn < len(turns_err) else None
                    out = turns_out[turn] if turn < len(turns_out) else None
                    if err is None and out is None:
                        continue
                    f.write(f"\n--- turn {turn} ---\n")
                    if err:
                        f.write(err if err.endswith("\n") else err + "\n")
                    if out:
                        f.write(f">>> {out.strip()}\n")
        os.remove(json_path)
    except Exception:
        pass  # logs are best-effort; don't fail the game

    return seed, score1, score2, winner


def main():
    parser = argparse.ArgumentParser(description="Run N Troll Farm battles between two bots.")
    parser.add_argument("bot1",                              help="Path to bot 1 executable")
    parser.add_argument("bot2",                              help="Path to bot 2 executable")
    parser.add_argument("-n", "--num-games", type=int, default=20, metavar="N",
                        help="Number of games (default: 20)")
    parser.add_argument("-l", "--league",    type=int, default=3,  metavar="L",
                        help="League level (default: 3)")
    parser.add_argument("-j", "--jobs",      type=int, default=4,  metavar="J",
                        help="Parallel games (default: 4)")
    parser.add_argument("-s", "--seed",      type=int, default=None,
                        help="First seed; sequential if set, random otherwise")
    args = parser.parse_args()

    # Create run folder
    ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    run_dir = os.path.join(RUNS_DIR, ts)
    os.makedirs(run_dir, exist_ok=True)

    base = args.seed if args.seed is not None else random.randint(1, 10**6)
    seeds = [base + i for i in range(args.num_games)]

    args.bot1 = os.path.abspath(args.bot1)
    args.bot2 = os.path.abspath(args.bot2)
    b1 = os.path.basename(args.bot1)
    b2 = os.path.basename(args.bot2)
    print(f"Running {args.num_games} games  {b1} vs {b2}  league={args.league}  jobs={args.jobs}  base_seed={base}")
    print(f"Logs → {run_dir}/\n")

    results = []
    errors  = 0

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {
            pool.submit(run_game, args.bot1, args.bot2, args.league, s, run_dir): s
            for s in seeds
        }
        completed = 0
        for fut in as_completed(futures):
            seed, s1, s2, winner = fut.result()
            completed += 1
            if winner in ("TIMEOUT", "PARSE_ERROR"):
                errors += 1
                print(f"  [{completed:3d}/{args.num_games}] seed={seed:8d}  ERROR ({winner})")
            else:
                results.append({"seed": seed, "score1": s1, "score2": s2, "winner": winner})
                marker = "<" if winner == "P1" else (">" if winner == "P2" else "=")
                print(f"  [{completed:3d}/{args.num_games}] seed={seed:8d}  {s1:4d} {marker} {s2:4d}  {winner}")

    n     = len(results)
    wins1 = sum(1 for r in results if r["winner"] == "P1")
    wins2 = sum(1 for r in results if r["winner"] == "P2")
    draws = sum(1 for r in results if r["winner"] == "DRAW")

    avg1  = sum(r["score1"] for r in results) / n if n else 0
    avg2  = sum(r["score2"] for r in results) / n if n else 0

    summary_lines = [
        "",
        f"=== Results — {n} games, {errors} errors ===",
        f"{'Bot':<20} {'Wins':>6} {'Win%':>7} {'AvgScore':>10}",
        f"{'-'*46}",
        f"{b1:<20} {wins1:>6} {100*wins1/n if n else 0:>6.1f}% {avg1:>10.1f}",
        f"{b2:<20} {wins2:>6} {100*wins2/n if n else 0:>6.1f}% {avg2:>10.1f}",
    ]
    if draws:
        summary_lines.append(f"{'Draws':<20} {draws:>6}")
    summary_lines.append("")

    summary_text = "\n".join(summary_lines)
    print(summary_text)

    with open(os.path.join(run_dir, "summary.txt"), "w") as f:
        f.write(f"bot1={args.bot1}\nbot2={args.bot2}\n")
        f.write(f"league={args.league}  n_games={args.num_games}\n")
        f.write(summary_text)

    with open(os.path.join(run_dir, "summary.json"), "w") as f:
        json.dump({
            "bot1": args.bot1, "bot2": args.bot2,
            "league": args.league, "n_games": args.num_games,
            "wins1": wins1, "wins2": wins2, "draws": draws, "errors": errors,
            "winrate1": wins1 / n if n else 0,
            "winrate2": wins2 / n if n else 0,
            "avg_score1": avg1, "avg_score2": avg2,
            "games": sorted(results, key=lambda r: r["seed"]),
        }, f, indent=2)


if __name__ == "__main__":
    main()
