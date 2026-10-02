#!/usr/bin/env python3
"""Black-box tests for ./train and ./predict.

Usage (from the repo root, after `make all`):
    python3 test/run_tests.py            # all tests
    python3 test/run_tests.py -v         # also show program output on failure

Datasets come from test/datasets (regenerate with test/gen_datasets.py).
Each run happens in a throwaway directory, so the repo's vars.csv / graph.svg
are never touched. xdg-open is replaced by a no-op so no browser pops up.

Note: this file was written with the help of an AI assistant (Claude).
"""
import atexit
import csv
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DATA = os.path.join(HERE, "datasets")
TRAIN = os.path.join(ROOT, "train")
PREDICT = os.path.join(ROOT, "predict")
VERBOSE = "-v" in sys.argv
TIMEOUT = 20

GREEN, RED, DIM, RESET = ("\033[32m", "\033[31m", "\033[2m", "\033[0m") if sys.stdout.isatty() else ("",) * 4
results = []


def report(name, ok, detail="", output=""):
    results.append(ok)
    tag = f"{GREEN}PASS{RESET}" if ok else f"{RED}FAIL{RESET}"
    print(f"[{tag}] {name:<34} {DIM}{detail}{RESET}")
    if not ok and VERBOSE and output:
        print("\n".join("    | " + l for l in output.strip().splitlines()[-15:]))


def sandbox():
    """Fresh working dir + PATH with a fake xdg-open."""
    d = tempfile.mkdtemp(prefix="linreg_test_")
    # safety net: removed at exit even if a test blows up before its rmtree
    atexit.register(shutil.rmtree, d, ignore_errors=True)
    bindir = os.path.join(d, ".bin")
    os.mkdir(bindir)
    fake = os.path.join(bindir, "xdg-open")
    with open(fake, "w") as f:
        f.write("#!/bin/sh\nexit 0\n")
    os.chmod(fake, 0o755)
    env = dict(os.environ, PATH=bindir + os.pathsep + os.environ.get("PATH", ""))
    return d, env


def run(cmd, cwd, env, stdin=""):
    try:
        p = subprocess.run(cmd, cwd=cwd, env=env, input=stdin, capture_output=True,
                           text=True, errors="replace", timeout=TIMEOUT)
        return p.returncode, p.stdout + p.stderr, False
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or b"").decode(errors="replace") if isinstance(e.stdout, bytes) else (e.stdout or "")
        return None, out, True


def read_vars(cwd):
    try:
        with open(os.path.join(cwd, "vars.csv")) as f:
            a, b = f.read().strip().split(",")[:2]
            return float(a), float(b)
    except (OSError, ValueError):
        return None


def load_points(path):
    """Rows whose first two comma-separated fields are plain numbers.
    Read as bytes: csv.reader chokes on NUL, and a row containing a NUL
    (or hex, or trailing junk) must count as malformed, not crash the test."""
    pts = []
    with open(path, "rb") as f:
        for line in f.read().split(b"\n"):
            row = line.split(b",")
            try:
                a, b = (r.decode("ascii") for r in row[:2])
                if "\0" in a + b or "x" in (a + b).lower() or "_" in a + b:
                    continue  # float() would accept hex-ish/underscored; C won't
                pts.append((float(a), float(b)))
            except (ValueError, IndexError, UnicodeDecodeError):
                pass
    return pts


def predict_values(cwd, env, xs):
    """Feed xs to ./predict, return (rc, out, timeout, list of y it printed)."""
    rc, out, timeout = run([PREDICT], cwd, env, "".join(f"{x!r}\n" for x in xs))
    return rc, out, timeout, [float(v) for v in re.findall(r"y\s*=\s*(\S+)", out)]


def crashed(out, rc):
    """Real crash (not a leak report, which is checked separately).
    A negative rc means killed by a signal: catches crashes in non-ASan builds,
    which print nothing (the "Segmentation fault" line comes from the shell)."""
    return ((rc is not None and rc < 0) or "ERROR: AddressSanitizer" in out
            or "Segmentation fault" in out or "runtime error" in out)


def leaked(out):
    return "LeakSanitizer" in out


# --------------------------------------------------------------------------
def test_predict_before_train():
    d, env = sandbox()
    rc, out, timeout, ys = predict_values(d, env, [0, 1000, 240000])
    why = health(rc, out, timeout)
    ok = why is None and ys == [0.0, 0.0, 0.0]
    report("predict without vars.csv -> 0", ok, why or f"got {ys}", out)
    shutil.rmtree(d)


def test_predict_bad_input():
    d, env = sandbox()
    with open(os.path.join(d, "vars.csv"), "w") as f:
        f.write("100,2\n")
    rc, out, timeout = run([PREDICT], d, env, "5\nabc\n\n")
    ys = [float(v) for v in re.findall(r"y\s*=\s*(\S+)", out)]
    # only the valid "5" line should produce a value (110)
    ok = not timeout and not crashed(out, rc) and ys == [110.0]
    report("predict rejects non-numeric input", ok, f"values printed: {ys}", out)
    shutil.rmtree(d)


# --------------------------------------------------------------------------
# predict input hardening
#
# Contract checked here:
#   * one input line -> exactly one answer: "y = <finite number>" or an error
#     message (no "y =" at all). Never two answers for one line.
#   * a line is valid iff, after trimming surrounding whitespace (incl. \r),
#     it is ONE plain decimal number (sign, digits, '.', exponent allowed)
#     and finite. Negative x is fine: this is a generic linear regression.
#     Everything else is rejected: trailing junk, hex, inf/nan, overflow, ...
#   * a computed y that is not finite is not printed.
#   * vars.csv must be exactly "<finite>,<finite>" (+ optional newline),
#     anything else -> refuse (non-zero exit, no y printed). Missing -> 0,0.
#   * EOF ends the program with exit code 0; no crash / hang / leak, ever.

def run_bytes(cmd, cwd, env, stdin=b""):
    try:
        p = subprocess.run(cmd, cwd=cwd, env=env, input=stdin, capture_output=True,
                           timeout=TIMEOUT)
        out = (p.stdout + p.stderr).decode(errors="replace")
        return p.returncode, out, False
    except subprocess.TimeoutExpired as e:
        return None, (e.stdout or b"").decode(errors="replace"), True


def parse_ys(out):
    ys = []
    for v in re.findall(r"y\s*=\s*(\S+)", out):
        try:
            ys.append(float(v))
        except ValueError:
            ys.append(float("nan"))  # printed something that isn't a number
    return ys


def predict_raw(vars_content, stdin):
    """Run predict once in a sandbox. vars_content: None (no file), str/bytes."""
    d, env = sandbox()
    if vars_content is not None:
        mode = "wb" if isinstance(vars_content, bytes) else "w"
        with open(os.path.join(d, "vars.csv"), mode) as f:
            f.write(vars_content)
    if isinstance(stdin, str):
        stdin = stdin.encode()
    rc, out, timeout = run_bytes([PREDICT], d, env, stdin)
    shutil.rmtree(d)
    return rc, out, timeout, parse_ys(out)


def health(rc, out, timeout):
    """Return a failure reason, or None if the run was clean."""
    if timeout:
        return "timeout"
    if crashed(out, rc):
        return "crash"
    if leaked(out):
        return "leak"
    return None


VARS = "100,2\n"           # y = 100 + 2x


def y_of(x):
    return 100 + 2 * x


# (label, line sent, expected x)
PREDICT_VALID = [
    ("integer",              "42",          42),
    ("zero",                 "0",           0),
    ("decimal",              "42.5",        42.5),
    ("leading dot",          ".5",          0.5),
    ("trailing dot",         "5.",          5),
    ("explicit plus",        "+42",         42),
    ("leading zeros",        "000042",      42),
    ("exponent",             "1e5",         1e5),
    ("exponent upper",       "1E5",         1e5),
    ("exponent signed",      "2.5e+3",      2500),
    ("big mileage",          "999999999",   999999999),
    ("leading spaces",       "   42",       42),
    ("trailing spaces",      "42   ",       42),
    ("tabs",                 "\t42\t",      42),
    ("negative",             "-5",          -5),
    ("negative decimal",     "-0.001",      -0.001),
    ("CRLF",                 "42\r",        42),
]

# (label, line sent)
PREDICT_INVALID = [
    ("empty line",           ""),
    ("only spaces",          "   "),
    ("letters",              "abc"),
    ("trailing letters",     "5abc"),
    ("trailing letter after space", "5 a"),
    ("two numbers",          "5 5"),
    ("comma separated",      "5,5"),
    ("thousands space",      "240 000"),
    ("thousands comma",      "240,000"),
    ("comma decimal",        "42,5"),
    ("two dots",             "5.5.5"),
    ("double minus",         "--5"),
    ("plus minus",           "+-5"),
    ("lone sign",            "-"),
    ("lone plus",            "+"),
    ("lone dot",             "."),
    ("exponent only",        "e5"),
    ("dangling exponent",    "1e"),
    ("dangling exp sign",    "1e+"),
    ("hex",                  "0x10"),
    ("hex float",            "0x1p3"),
    ("inf",                  "inf"),
    ("-inf",                 "-inf"),
    ("infinity",             "infinity"),
    ("nan",                  "nan"),
    ("NaN",                  "NaN"),
    ("overflow",             "1e400"),
    ("negative overflow",    "-1e400"),
    ("fullwidth digit",      "５"),
    ("arrow key escape",     "\x1b[A"),
    ("trailing dollar",      "42$"),
    ("units",                "42km"),
    ("quoted",               "\"42\""),
]


def test_predict_valid_inputs():
    for label, line, x in PREDICT_VALID:
        rc, out, timeout, ys = predict_raw(VARS, line + "\n")
        why = health(rc, out, timeout)
        ok = why is None and len(ys) == 1 and abs(ys[0] - y_of(x)) <= 1e-6 * max(1, abs(y_of(x)))
        report(f"predict accepts {label}", ok, why or f"{line!r} -> {ys}, want [{y_of(x)}]", out)


def test_predict_invalid_inputs():
    for label, line in PREDICT_INVALID:
        rc, out, timeout, ys = predict_raw(VARS, line + "\n")
        why = health(rc, out, timeout)
        ok = why is None and ys == []
        report(f"predict rejects {label}", ok, why or f"{line!r} -> {ys}", out)


def test_predict_line_handling():
    # program keeps going after a bad line, each line answered independently
    rc, out, timeout, ys = predict_raw(VARS, "abc\n5\n5abc\n\n7\n")
    why = health(rc, out, timeout)
    report("predict recovers after bad lines", why is None and ys == [y_of(5), y_of(7)],
           why or f"got {ys}, want {[y_of(5), y_of(7)]}", out)

    # NUL byte hidden in the line: C string functions stop at it
    rc, out, timeout, ys = predict_raw(VARS, b"5\x00abc\n")
    why = health(rc, out, timeout)
    report("predict rejects NUL in line", why is None and ys == [], why or f"got {ys}", out)

    rc, out, timeout, ys = predict_raw(VARS, b"\x00\n")
    why = health(rc, out, timeout)
    report("predict rejects lone NUL", why is None and ys == [], why or f"got {ys}", out)

    # NUL right before the newline: strtod stops at it and sees "5" + end of
    # string, so only a length check (getline's return vs strlen) catches it
    rc, out, timeout, ys = predict_raw(VARS, b"5\x00\n")
    why = health(rc, out, timeout)
    report("predict rejects NUL before newline", why is None and ys == [], why or f"got {ys}", out)

    rc, out, timeout, ys = predict_raw(VARS, b"\x005\n")
    why = health(rc, out, timeout)
    report("predict rejects NUL before number", why is None and ys == [], why or f"got {ys}", out)

    rc, out, timeout, ys = predict_raw(VARS, b"5\x00")
    why = health(rc, out, timeout)
    report("predict rejects NUL at EOF", why is None and ys == [], why or f"got {ys}", out)

    rc, out, timeout, ys = predict_raw(VARS, b"5\n\x00\n7\n")
    why = health(rc, out, timeout)
    report("predict NUL line between good ones", why is None and ys == [y_of(5), y_of(7)],
           why or f"got {ys}, want {[y_of(5), y_of(7)]}", out)

    # binary garbage / invalid UTF-8
    rc, out, timeout, ys = predict_raw(VARS, b"\xff\xfe\x80\n")
    why = health(rc, out, timeout)
    report("predict rejects binary garbage", why is None and ys == [], why or f"got {ys}", out)

    # lines longer than any fixed buffer must still be ONE line
    long_valid = "0" * 2000 + "5"
    rc, out, timeout, ys = predict_raw(VARS, long_valid + "\n")
    why = health(rc, out, timeout)
    ok = why is None and ys in ([], [y_of(5)])  # answering 5 or rejecting are both fine
    report("predict long line = one answer", ok, why or f"2001-char '0..05' -> {ys}", out)

    rc, out, timeout, ys = predict_raw(VARS, "5" + " " * 2000 + "x\n")
    why = health(rc, out, timeout)
    report("predict long line junk at end", why is None and ys == [],
           why or f"'5<2000 spaces>x' -> {ys}", out)

    rc, out, timeout, ys = predict_raw(VARS, "1" * 2000 + "\n")
    why = health(rc, out, timeout)
    report("predict long line overflowing", why is None and ys == [],
           why or f"2000 x '1' -> {ys}", out)

    rc, out, timeout, ys = predict_raw(VARS, "x" * 100000 + "\n7\n")
    why = health(rc, out, timeout)
    report("predict 100k junk line then 7", why is None and ys == [y_of(7)],
           why or f"got {ys}, want [{y_of(7)}]", out)

    # last line without newline still gets answered
    rc, out, timeout, ys = predict_raw(VARS, "42")
    why = health(rc, out, timeout)
    report("predict last line without newline", why is None and ys == [y_of(42)],
           why or f"got {ys}", out)

    # empty stdin: clean exit 0
    rc, out, timeout, ys = predict_raw(VARS, "")
    why = health(rc, out, timeout)
    report("predict empty stdin exits 0", why is None and rc == 0 and ys == [],
           why or f"rc={rc} ys={ys}", out)

    # many lines: no slowdown / leak per line
    n = 20000
    rc, out, timeout, ys = predict_raw(VARS, "".join(f"{i}\n" for i in range(n)))
    why = health(rc, out, timeout)
    ok = why is None and rc == 0 and len(ys) == n and ys[-1] == y_of(n - 1)
    report(f"predict {n} lines", ok, why or f"rc={rc} answers={len(ys)}", out)


def test_predict_output_overflow():
    # valid x, valid thetas, but theta1 * x overflows: must not print inf/nan
    rc, out, timeout, ys = predict_raw("0,1e300\n", "1e10\n")
    why = health(rc, out, timeout)
    ok = why is None and all(math.isfinite(y) for y in ys)
    report("predict hides non-finite result", ok, why or f"got {ys}", out)


# (label, vars.csv content, x, expected y) -- must work
VARS_VALID = [
    ("no trailing newline",  "100,2",                     5, 110),
    ("trailing newline",     "100,2\n",                   5, 110),
    ("CRLF",                 "100,2\r\n",                 5, 110),
    ("exponent (%.17g)",     "1e3,2.5e-2\n",              100, 1002.5),
    ("tiny exponent",        "5000,7.7108186204474013e-20\n", 1000, 5000),
    ("negative thetas",      "-100,-2\n",                 5, -110),
    ("full precision",       "8499.5996498915138,-0.021448963591305697\n", 100000,
     8499.5996498915138 - 0.021448963591305697 * 100000),
]

# (label, vars.csv content) -- must refuse: non-zero exit and no y printed
VARS_INVALID = [
    ("empty file",           ""),
    ("only newline",         "\n"),
    ("garbage",              "garbage\n"),
    ("one value",            "100\n"),
    ("missing second",       "100,\n"),
    ("missing first",        ",2\n"),
    ("trailing junk",        "100,2abc\n"),
    ("junk in first",        "100abc,2\n"),
    ("three values",         "100,2,3\n"),
    ("semicolon",            "100;2\n"),
    ("space separated",      "100 2\n"),
    ("second line",          "100,2\n3,4\n"),
    ("nan",                  "nan,2\n"),
    ("inf",                  "100,inf\n"),
    ("-inf",                 "-inf,2\n"),
    ("overflow",             "1e400,2\n"),
    ("hex",                  "0x64,2\n"),
    ("NUL inside",           b"100,2\x00junk\n"),
    ("NUL before newline",   b"100,2\x00\n"),
    ("NUL as second line",   b"100,2\n\x00"),
    ("blank second line",    "100,2\n\n"),
    ("binary",               b"\xff\xfe\x00\x01"),
]


def test_predict_vars_valid():
    for label, content, x, y in VARS_VALID:
        rc, out, timeout, ys = predict_raw(content, f"{x!r}\n")
        why = health(rc, out, timeout)
        ok = why is None and len(ys) == 1 and abs(ys[0] - y) <= 1e-6 * max(1, abs(y))
        report(f"vars.csv ok: {label}", ok, why or f"{content!r} x={x} -> {ys}, want [{y}]", out)


def test_predict_vars_invalid():
    for label, content in VARS_INVALID:
        rc, out, timeout, ys = predict_raw(content, "5\n")
        why = health(rc, out, timeout)
        ok = why is None and rc != 0 and ys == []
        report(f"vars.csv refused: {label}", ok, why or f"{content!r} -> rc={rc} ys={ys}", out)

    # vars.csv is a directory
    d, env = sandbox()
    os.mkdir(os.path.join(d, "vars.csv"))
    rc, out, timeout = run_bytes([PREDICT], d, env, b"5\n")
    ys = parse_ys(out)
    why = health(rc, out, timeout)
    report("vars.csv refused: is a directory", why is None and rc != 0 and ys == [],
           why or f"rc={rc} ys={ys}", out)
    shutil.rmtree(d)

    # vars.csv unreadable (meaningless as root, who can read anything)
    if os.geteuid() != 0:
        d, env = sandbox()
        p = os.path.join(d, "vars.csv")
        with open(p, "w") as fh:
            fh.write("100,2\n")
        os.chmod(p, 0)
        rc, out, timeout = run_bytes([PREDICT], d, env, b"5\n")
        ys = parse_ys(out)
        why = health(rc, out, timeout)
        report("vars.csv refused: unreadable", why is None and rc != 0 and ys == [],
               why or f"rc={rc} ys={ys}", out)
        os.chmod(p, 0o644)
        shutil.rmtree(d)


def test_good_dataset(path, name, t0_exp, t1_exp):
    d, env = sandbox()
    rc, out, timeout = run([TRAIN, path], d, env)
    got = read_vars(d)
    if timeout or crashed(out, rc) or got is None or rc != 0:
        why = ("timeout" if timeout else "crash" if crashed(out, rc)
               else "no vars.csv" if got is None else f"saved vars.csv but exited rc={rc}")
        report(f"train {name}", False, why, out)
        shutil.rmtree(d)
        return
    # compare predictions over the data range, relative to the spread of y:
    # scale-independent, and catches precision loss in vars.csv
    pts = load_points(path)
    xs = [x for x, _ in pts]
    ys = [y for _, y in pts]
    yrange = (max(ys) - min(ys)) or 1.0
    probes = [min(xs), (min(xs) + max(xs)) / 2, max(xs)]
    err = max(abs((got[0] + got[1] * x) - (t0_exp + t1_exp * x)) for x in probes) / yrange
    ok = math.isfinite(err) and err < 1e-4 and not leaked(out)
    report(f"train {name}", ok,
           ("MEMORY LEAK, " if leaked(out) else "") + f"theta=({got[0]:.6g}, {got[1]:.6g}) expected=({t0_exp:.6g}, {t1_exp:.6g}) relerr={err:.1e}",
           out)
    # end-to-end: what predict prints must match the formula
    if ok:
        _, pout, _, got_y = predict_values(d, env, probes)
        exp_y = [t0_exp + t1_exp * x for x in probes]
        ok2 = len(got_y) == len(exp_y) and all(
            abs(a - b) / yrange < 1e-4 for a, b in zip(got_y, exp_y))
        report(f"  predict after {name}", ok2, f"got {got_y}", pout)
    shutil.rmtree(d)


def ols(pts):
    pts = [(x, y) for x, y in pts if math.isfinite(x) and math.isfinite(y)]
    if len(pts) < 2:
        return None
    # work on x / max|x| so the reference itself can't overflow on huge data
    sx = max(abs(x) for x, _ in pts) or 1.0
    n = len(pts)
    mx = sum(x / sx for x, _ in pts) / n
    my = sum(y for _, y in pts) / n
    sxx = sum((x / sx - mx) ** 2 for x, _ in pts)
    if sxx == 0 or not math.isfinite(sxx):
        return None
    t1 = sum((x / sx - mx) * (y - my) for x, y in pts) / sxx
    return my - t1 * mx, t1 / sx


def test_edge_dataset(name):
    """Rule: either refuse (non-zero exit + message), or save the RIGHT model
    (least squares over every numeric row). Never crash, hang, leak or save nan."""
    path = os.path.join(DATA, name)
    d, env = sandbox()
    rc, out, timeout = run([TRAIN, path], d, env)
    got = read_vars(d)
    want = ols(load_points(path))
    if timeout:
        ok, why = False, "timeout (infinite loop?)"
    elif "LeakSanitizer" in out:
        ok, why = False, "memory leak"
    elif crashed(out, rc):
        m = re.search(r"AddressSanitizer: (\S+)", out)
        ok, why = False, "crash: " + (m.group(1) if m else "signal")
    elif got is not None and not all(math.isfinite(v) for v in got):
        ok, why = False, f"saved non-finite thetas {got}"
    elif got is not None and rc != 0:
        ok, why = False, f"exited rc={rc} but still saved vars.csv {got}"
    elif got is not None and rc == 0:
        if want is None:
            ok, why = False, f"saved {got} but data has no valid fit: should refuse"
        else:
            # same scale-free check as the good datasets
            xs = [x for x, _ in load_points(path) if math.isfinite(x)]
            ys = [y for _, y in load_points(path) if math.isfinite(y)]
            yr = (max(ys) - min(ys)) or 1.0
            err = max(abs((got[0] + got[1] * x) - (want[0] + want[1] * x)) for x in (min(xs), max(xs))) / yr
            ok = err < 1e-4
            why = f"saved {got}, least squares on all rows = ({want[0]:.6g}, {want[1]:.6g})"
    else:
        ok, why = rc != 0, f"refused, rc={rc}"
    report(f"edge {name}", ok, why, out)
    shutil.rmtree(d)


def main():
    for b in (TRAIN, PREDICT):
        if not os.access(b, os.X_OK):
            sys.exit(f"{b} not found: run `make all` first")
    if not os.path.exists(os.path.join(DATA, "expected.csv")):
        subprocess.run([sys.executable, os.path.join(HERE, "gen_datasets.py")], check=True)

    print("== predict ==")
    test_predict_before_train()
    test_predict_bad_input()
    test_predict_line_handling()
    test_predict_output_overflow()

    print("== predict: valid inputs ==")
    test_predict_valid_inputs()
    print("== predict: invalid inputs (must print no y) ==")
    test_predict_invalid_inputs()
    print("== predict: vars.csv ==")
    test_predict_vars_valid()
    test_predict_vars_invalid()

    print("== train on datasets with a known answer ==")
    expected = {}
    with open(os.path.join(DATA, "expected.csv")) as f:
        for row in csv.DictReader(f):
            expected[row["file"]] = (float(row["theta0"]), float(row["theta1"]))
    for name, (t0, t1) in expected.items():
        test_good_dataset(os.path.join(DATA, name), name, t0, t1)
    # the subject's dataset, checked against closed-form least squares
    subj = os.path.join(HERE, "data.csv")
    if os.path.exists(subj):
        pts = load_points(subj)
        n = len(pts)
        mx = sum(x for x, _ in pts) / n
        my = sum(y for _, y in pts) / n
        t1 = sum((x - mx) * (y - my) for x, y in pts) / sum((x - mx) ** 2 for x, _ in pts)
        test_good_dataset(subj, "subject data.csv", my - t1 * mx, t1)

    print("== edge cases (must not crash / hang / save nan) ==")
    for name in sorted(os.listdir(DATA)):
        if name.endswith(".csv") and name != "expected.csv" and name not in expected:
            test_edge_dataset(name)

    passed = sum(results)
    print(f"\n{passed}/{len(results)} passed")
    sys.exit(0 if passed == len(results) else 1)


if __name__ == "__main__":
    main()
