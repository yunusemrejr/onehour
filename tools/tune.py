#!/usr/bin/env python3
"""One Hour - self-play training and evaluation of the computer commander.

  eval: plays the commander with some settings against the commander before it (--evalai, many headless series at once) and adds up the results
  cem:  trains the commander's numbers (TuneId in src/ai.h) by the cross-entropy method: every generation plays a handful of candidate sets on the
        same seeds against the hand-set commander, keeps the best few and draws the next candidates round them

Examples
  python3 tools/tune.py eval --env ONEHOUR_NEWMASK=<mask> --env ONEHOUR_BASEMASK=<mask>     (masks: AiFeat bits, src/ai.h)
  python3 tools/tune.py cem --mask 2147479551 --gens 5 --pop 12 --elite 4 --series 12 --games 4 --out tune-log.txt

The new side plays with FEAT_TUNED in its mask and the candidate in ONEHOUR_TUNE; the baseline plays the hand-set numbers (2147479551 is
FEAT_DEFAULT without FEAT_TUNED). The shipped TUNE_TRAINED came from 5 generations x 12 candidates x 48 Brutal matches (series 101 on), with
--mask 2147454975 (the commander before the learning update) and --newmask 2147463167 (plus FEAT_PUNISH); a generation takes about 20 minutes on
12 threads. Evaluate on other seeds (series 1-20) than the training used. Every run points ONEHOUR_BRAIN at a missing file, so the player's own
learned brain is never read or written.
"""
import argparse, math, os, random, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
FEAT_TUNED = 0x1000

# name: (hand-set value, sigma of the first generation, lowest, highest) - the trainer's search space
SPACE = {
    "thr":       (1.0, 0.18, 0.5, 1.8),
    "first":     (1.0, 0.18, 0.5, 1.8),
    "launch":    (0.0, 0.6, -2.0, 2.0),
    "cap":       (1.0, 0.18, 0.6, 1.8),
    "def":       (1.0, 0.25, 0.3, 2.5),
    "walls":     (1.0, 0.25, 0.2, 2.0),
    "reassess":  (0.8, 0.15, 0.4, 1.4),
    "retreat":   (1.9, 0.35, 1.1, 3.5),
    "reinforce": (0.6, 0.15, 0.25, 1.2),
    "guard":     (0.0, 0.06, 0.0, 0.35),
    "harv":      (1.0, 0.15, 0.6, 1.6),
    "counter":   (1.2, 0.3, 0.0, 2.4),
    "punishThr": (0.5, 0.12, 0.2, 1.0),
    "punishRa":  (1.15, 0.2, 0.7, 2.0),
}


def run_series(binary, seed, games, d0, env):
    e = dict(os.environ)
    e["ONEHOUR_BRAIN"] = "/nonexistent/onehour-brain.txt"
    e.update(env)
    out = subprocess.run([binary, "--seed", str(seed), "--d0", str(d0), "--evalai", str(games)], env=e, capture_output=True, text=True).stdout
    m = re.search(r"(\d+) wins, (\d+) losses, (\d+) unresolved", out)
    t = re.search(r"tiebreak at the limit .*?: (\d+) new, (\d+) old", out)
    c = re.search(r"counterattacks by the new commander: (\d+)", out)
    if not m:
        raise RuntimeError("no result from seed %d:\n%s" % (seed, out[-2000:]))
    return [int(m.group(1)), int(m.group(2)), int(m.group(3)), int(t.group(1)) if t else 0, int(t.group(2)) if t else 0, int(c.group(1)) if c else 0]


def evaluate(binary, seeds, games, d0, env, jobs):
    tot = [0] * 6
    with ThreadPoolExecutor(max_workers=jobs) as ex:
        for r in ex.map(lambda s: run_series(binary, s, games, d0, env), seeds):
            tot = [a + b for a, b in zip(tot, r)]
    return tot


def score(t):
    """Wins count 1, losses -1, an unresolved match half a point to whoever holds clearly more material at the limit."""
    w, l, u, tw, tl, _ = t
    n = max(1, w + l + u)
    return (w - l + 0.5 * (tw - tl)) / n


def seed_list(n, mul, add, start=1):
    return [k * mul + add for k in range(start, start + n)]


def cmd_eval(a):
    env = dict(kv.split("=", 1) for kv in a.env)
    t = evaluate(a.bin, seed_list(a.series, a.seedmul, a.seedadd, a.start), a.games, a.d0, env, a.jobs)
    print("new %d - old %d, unresolved %d (material at the limit: new %d, old %d), counterattacks %d | score %+.3f" % (t[0], t[1], t[2], t[3], t[4], t[5], score(t)))


def cmd_cem(a):
    names = [n for n in (a.params.split(",") if a.params else SPACE.keys())]
    mean = {n: SPACE[n][0] for n in names}
    sig = {n: SPACE[n][1] for n in names}
    if a.init:
        for kv in a.init.split(","):
            k, v = kv.split("="); mean[k] = float(v)
    rng = random.Random(a.rseed)
    log = open(a.out, "a") if a.out else None
    def say(s):
        print(s); sys.stdout.flush()
        if log: log.write(s + "\n"); log.flush()
    mask = int(a.newmask or a.mask) | FEAT_TUNED
    base = {"ONEHOUR_NEWMASK": str(mask), "ONEHOUR_BASEMASK": str(int(a.mask))}
    for g in range(a.gens):
        seeds = seed_list(a.series, a.seedmul, a.seedadd, 1 + g * a.series + a.offset)   # fresh seeds every generation, the same for its candidates
        cands = [dict(mean)]   # the current mean is always one of them (how good is it on this generation's seeds?)
        while len(cands) < a.pop:
            c = {}
            for n in names:
                v0, _, lo, hi = SPACE[n]
                c[n] = min(hi, max(lo, rng.gauss(mean[n], sig[n])))
            cands.append(c)
        # every series of every candidate in one pool: the cores stay busy until the generation's last match
        envs = []
        for c in cands:
            env = dict(base); env["ONEHOUR_TUNE"] = ",".join("%s=%.4f" % (k, v) for k, v in c.items()); envs.append(env)
        tasks = [(i, sd) for i in range(len(cands)) for sd in seeds]
        tots = [[0] * 6 for _ in cands]
        with ThreadPoolExecutor(max_workers=a.jobs) as ex:
            for (i, sd), r in zip(tasks, ex.map(lambda t: run_series(a.bin, t[1], a.games, a.d0, envs[t[0]]), tasks)):
                tots[i] = [x + y for x, y in zip(tots[i], r)]
        results = []
        for i, c in enumerate(cands):
            t = tots[i]
            results.append((score(t), i, c, t))
            say("gen %d cand %d score %+.3f  (%d-%d, %d unresolved, tiebreak %d-%d, counterattacks %d)  %s" % (g, i, score(t), t[0], t[1], t[2], t[3], t[4], t[5], envs[i]["ONEHOUR_TUNE"]))
        results.sort(key=lambda r: -r[0])
        elite = [r[2] for r in results[:a.elite]]
        for n in names:
            vals = [e[n] for e in elite]
            m = sum(vals) / len(vals)
            sd = math.sqrt(sum((v - m) ** 2 for v in vals) / len(vals))
            mean[n] = m
            sig[n] = max(SPACE[n][1] * 0.25, 0.6 * sig[n] + 0.4 * sd)
        say("gen %d -> mean %s" % (g, ",".join("%s=%.4f" % (k, v) for k, v in mean.items())))
    say("final %s" % ",".join("%s=%.4f" % (k, v) for k, v in mean.items()))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("mode", choices=["eval", "cem"])
    p.add_argument("--bin", default=os.path.join(HERE, "..", "build", "onehour"))
    p.add_argument("--d0", type=int, default=3, help="difficulty of both sides (3 Brutal, 2 Hard)")
    p.add_argument("--games", type=int, default=8, help="matches per series (one process)")
    p.add_argument("--series", type=int, default=12, help="series per evaluation (or per candidate)")
    p.add_argument("--jobs", type=int, default=12)
    p.add_argument("--seedmul", type=int, default=211)
    p.add_argument("--seedadd", type=int, default=7)
    p.add_argument("--start", type=int, default=1)
    p.add_argument("--env", action="append", default=[], help="NAME=value for every run (eval)")
    p.add_argument("--mask", default="2147479551", help="cem: the feature mask the baseline plays with (default: FEAT_DEFAULT without FEAT_TUNED)")
    p.add_argument("--newmask", default="", help="cem: the new side's mask (default: --mask), FEAT_TUNED added")
    p.add_argument("--params", default="", help="cem: comma-separated names to train (default: all)")
    p.add_argument("--init", default="", help="cem: starting values name=v,...")
    p.add_argument("--gens", type=int, default=6)
    p.add_argument("--pop", type=int, default=8)
    p.add_argument("--elite", type=int, default=3)
    p.add_argument("--offset", type=int, default=100, help="cem: seeds start this many series in (keeps them apart from the evaluation seeds)")
    p.add_argument("--rseed", type=int, default=1)
    p.add_argument("--out", default="")
    a = p.parse_args()
    (cmd_eval if a.mode == "eval" else cmd_cem)(a)


if __name__ == "__main__":
    main()
