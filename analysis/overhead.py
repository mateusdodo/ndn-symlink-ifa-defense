#!/usr/bin/env python3
# Uso (raiz do repo): python3 analysis/overhead.py
import glob, statistics as st
rows = {}
for m in glob.glob("sim/results/dev/ovh/*/meta.txt"):
    d = dict(l.strip().split("=", 1) for l in open(m) if "=" in l)
    rows.setdefault((int(d["arm"]), int(d["fibN"]), int(d["suspN"])), []).append(d)

def per(rs, ns, n):
    return [int(r[ns]) / int(r[n]) for r in rs if int(r[n]) > 0]
def med(v): return st.median(v) if v else float("nan")
def fmt(v): return f"{med(v):7.0f} ({min(v):.0f}-{max(v):.0f})" if v else "      -"

print("(arm,fibN,suspN) reps | hint ns/chamada | normal ns/chamada | B hint | B normal | FIB igual")
for k in sorted(rows):
    rs = rows[k]
    h, n = per(rs, "nsHint", "nHint"), per(rs, "nsNormal", "nNormal")
    bh, bn = per(rs, "bytesHint", "nHint"), per(rs, "bytesNormal", "nNormal")
    same = all(r["fibBefore"] == r["fibAfter"] for r in rs)
    print(k, len(rs), "|", fmt(h), "|", fmt(n), "|", f"{med(bh):.1f}" if bh else "-",
          "|", f"{med(bn):.1f}" if bn else "-", "|", same)

base = rows.get((4, 0, 0))
if base:
    h0, n0 = med(per(base, "nsHint", "nHint")), med(per(base, "nsNormal", "nNormal"))
    print("\nrazao da mediana contra (4,0,0); 'constante' = dentro de [0.8, 1.25]")
    for k in sorted(rows):
        if k[0] != 4 or k == (4, 0, 0): continue
        rs = rows[k]
        print(k, f"hint x{med(per(rs,'nsHint','nHint'))/h0:.2f}", f"normal x{med(per(rs,'nsNormal','nNormal'))/n0:.2f}")
