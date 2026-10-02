#!/usr/bin/env python3
"""Generate test datasets for ft_linear_regression.

Usage: python3 gen_datasets.py [outdir]   (default: ./datasets next to this file)

Every "good" dataset has a known ground truth (theta0, theta1) written to
expected.csv, computed with the closed-form least-squares solution, so a
trained model can be checked against it (tolerance ~1e-3 relative is fair).
The "edge" datasets are meant to stress the parser / degenerate math.
Fixed seed: output is reproducible.

Note: this file was written with the help of an AI assistant (Claude).
"""
import os
import random
import sys

random.seed(42)
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "datasets")
os.makedirs(OUT, exist_ok=True)


def ols(points):
    n = len(points)
    mx = sum(x for x, _ in points) / n
    my = sum(y for _, y in points) / n
    sxx = sum((x - mx) ** 2 for x, _ in points)
    if sxx == 0:
        return None
    sxy = sum((x - mx) * (y - my) for x, y in points)
    t1 = sxy / sxx
    return my - t1 * mx, t1


def write(name, points, header="km,price", raw=None):
    path = os.path.join(OUT, name)
    with open(path, "w", newline="") as f:
        if raw is not None:
            f.write(raw)
        else:
            if header is not None:
                f.write(header + "\n")
            for x, y in points:
                f.write(f"{x!r},{y!r}\n")
    return path


def linear(n, t0, t1, xmin, xmax, noise, integer_x=True):
    pts = []
    for _ in range(n):
        x = random.randint(int(xmin), int(xmax)) if integer_x else random.uniform(xmin, xmax)
        y = t0 + t1 * x + random.gauss(0, noise)
        pts.append((x, round(y, 2)))
    return pts


expected = []

# --- "good" datasets: training should land on the OLS solution ---------------
good = {
    # exact line, no noise: thetas must be (almost) exactly 9000 / -0.025
    "perfect_line.csv": linear(30, 9000, -0.025, 10000, 250000, 0),
    # car-like data, similar to the subject's, with noise
    "cars_noisy.csv": linear(50, 8500, -0.021, 15000, 260000, 400),
    # 2500 rows: crosses the 1000-entry realloc boundary in parse_data
    "cars_big.csv": linear(2500, 8000, -0.02, 0, 300000, 500),
    # positive slope
    "positive_slope.csv": linear(40, 100, 3.5, 0, 1000, 50),
    # huge x, tiny slope: theta1 ~ 2e-7, lost if vars.csv is written with few decimals
    "tiny_slope.csv": linear(40, 50, 2.3e-7, 1_000_000, 50_000_000, 0.5),
    # negative and fractional x values
    "negative_x.csv": linear(40, -20, 1.5, -500, 500, 10, integer_x=False),
    # only two points: the line must go exactly through them
    "two_points.csv": [(10000, 8000), (200000, 4000)],
    # all points have the same y: slope must be 0, theta0 = that y
    "constant_y.csv": [(x, 5000) for x in range(10000, 200001, 10000)],
    # constant y equal to 0: range-widening tricks based on |y| do nothing at 0
    "zero_y.csv": [(x, 0) for x in range(10000, 200001, 10000)],
}
for name, pts in good.items():
    write(name, pts)
    t = ols(pts)
    expected.append((name, t))

# same data as perfect_line but with CRLF line endings (Windows export)
pts = good["perfect_line.csv"]
write("crlf.csv", None, raw="km,price\r\n" + "".join(f"{x},{y}\r\n" for x, y in pts))
expected.append(("crlf.csv", ols(pts)))

# same as cars_noisy, but with trailing empty lines at the end of the file
pts = good["cars_noisy.csv"]
write("trailing_newlines.csv", None, raw="km,price\n" + "".join(f"{x},{y}\n" for x, y in pts) + "\n\n")
expected.append(("trailing_newlines.csv", ols(pts)))

# --- other domains: not cars, different headers / scales / signs -----------
# (generated after the car datasets so those stay byte-identical)
others = {
    # exact physical law, fractional + negative x: F = 32 + 1.8 C
    "celsius_fahrenheit.csv": ("celsius,fahrenheit",
                               [(round(c, 3), round(32 + 1.8 * round(c, 3), 6))
                                for c in (random.uniform(-40, 100) for _ in range(25))]),
    "study_hours.csv": ("hours_studied,exam_score",
                        linear(60, 42, 4.8, 0, 12, 6, integer_x=False)),
    # negative slope, x in meters
    "altitude_boiling.csv": ("altitude_m,boiling_point_c",
                             linear(40, 100, -0.0033, 0, 8848, 0.3)),
    # header with spaces: label parsing must not choke
    "salary_years.csv": ("Years Of Experience,Annual Salary",
                         linear(80, 32000, 2600, 0, 35, 4000)),
    # x far from 0 (years): intercept is a big extrapolation, stresses
    # the denormalisation precision
    "sea_level.csv": ("year,sea_level_mm",
                      linear(120, -6200, 3.2, 1900, 2020, 8)),
    # negative y values
    "ocean_depth_temp.csv": ("depth_m,temperature_c",
                             linear(50, 2, -0.004, 0, 2000, 0.4)),
    # tiny magnitudes: written in exponent notation (1.2e-05 ...)
    "dose_response.csv": ("dose_g,response",
                          [(x / 1e5, y / 1e5) for x, y in linear(30, 3, 0.7, 1, 50, 0.5, integer_x=False)]),
    # very large x: GDP in dollars vs CO2 tonnes
    "gdp_co2.csv": ("gdp_usd,co2_tonnes",
                    linear(60, 5e6, 2.5e-4, 1e9, 2e13, 2e7)),
    # quoted header (spreadsheet export)
    "quoted_header.csv": ('"speed_kmh","stopping_distance_m"',
                          linear(40, -5, 0.9, 20, 130, 4)),
    # header that starts with digits: must still be seen as a header
    "digit_header.csv": ("1st_try,2nd_try",
                         linear(30, 10, 0.95, 0, 100, 5)),
    # rows not sorted by x, with repeated x values
    "unsorted_dupes.csv": ("hour,visitors",
                           [(h, v) for h, v in linear(80, 120, 15, 0, 23, 30)]),
}
for name, (header, pts) in others.items():
    write(name, pts, header=header)
    expected.append((name, ols(pts)))

# --- edge cases: no single right answer, but must not crash / hang / UB ------
edge = {
    "empty.csv": "",
    "header_only.csv": "km,price\n",
    "single_point.csv": "km,price\n100000,5000\n",
    # same x everywhere: slope undefined -> should be rejected with a message
    "constant_x.csv": "km,price\n" + "".join(f"50000,{p}\n" for p in (4000, 5000, 6000, 7000)),
    # same, with x == 0
    "zero_x.csv": "km,price\n0,4000\n0,5000\n0,6000\n",
    # no header: first data row must not be silently eaten as a header
    "no_header.csv": "".join(f"{x},{y}\n" for x, y in good["cars_noisy.csv"]),
    # garbage row in the middle: data after it must not be silently dropped
    "garbage_middle.csv": "km,price\n10000,8000\n20000,7500\nhello,world\n30000,6000\n40000,6200\n",
    # blank line in the middle
    "blank_middle.csv": "km,price\n10000,8000\n20000,7500\n\n30000,6000\n40000,6200\n",
    # missing price on a row
    "missing_value.csv": "km,price\n10000,8000\n20000,\n30000,7000\n",
    # extra column
    "extra_column.csv": "km,price,color\n10000,8000,red\n20000,7500,blue\n30000,7000,green\n",
    # header longer than the 256-byte label buffers
    "long_header.csv": "k" * 1000 + "," + "p" * 1000 + "\n10000,8000\n20000,7000\n",
    # values that strtod accepts but are not real numbers
    "nan_inf.csv": "km,price\n10000,8000\nnan,7000\n30000,inf\n40000,6000\n",
    # huge magnitudes: risk of overflow in sums/normalization
    "huge_values.csv": "km,price\n1e300,1\n2e300,2\n3e300,3\n",
    # European CSV: ';' separator, nothing parses -> should refuse
    "semicolon.csv": "km;price\n10000;8000\n20000;7000\n30000;6000\n",
    # space after the comma: fine to accept (and get it right) or refuse
    "space_after_comma.csv": "km, price\n10000, 8000\n20000, 7000\n30000, 6500\n",
    # spaces before the comma
    "space_before_comma.csv": "km ,price\n10000 ,8000\n20000 ,7000\n30000 ,6500\n",
    # NUL byte inside a row: "20000\0" + "9999" must not become (20000, ?)
    # (bad rows below use y values off the line of the good rows, so wrongly
    # keeping them changes the fit and gets caught)
    "nul_in_row.csv": "km,price\n10000,8000\n20000\x009999\n30000,7000\n40000,6500\n",
    # NUL right after a valid row: "30000,7000\0junk" is not a clean row
    "nul_trailing.csv": "km,price\n10000,8000\n20000,7600\n30000,7000\x00junk\n40000,6500\n",
    # trailing junk after the numbers
    "trailing_junk.csv": "km,price\n10000,8000\n20000,9999abc\n30000,7000\n40000,6500\n",
    # one-column header, data below is fine
    "one_column_header.csv": "km\n10000,8000\n20000,7000\n30000,6500\n",
    # hex in a data row (strtod/%lf accept it, it is still not a plain number)
    "hex_values.csv": "km,price\n10000,8000\n0x4e20,9999\n30000,7000\n40000,6500\n",
}
for name, raw in edge.items():
    write(name, None, raw=raw)

with open(os.path.join(OUT, "expected.csv"), "w") as f:
    f.write("file,theta0,theta1\n")
    for name, t in expected:
        f.write(f"{name},{t[0]!r},{t[1]!r}\n")

print(f"wrote {len(good) + len(others) + 2 + len(edge)} datasets + expected.csv to {OUT}")
