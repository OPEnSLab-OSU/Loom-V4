import argparse, csv, datetime as dt, glob, os, re, sys
from collections import defaultdict

HOUR = dt.timedelta(hours=1)
MAX_GAP = dt.timedelta(days=3)  # larger gaps are treated as the node being off, not missed readings

try:
    from zoneinfo import ZoneInfo
    PACIFIC = ZoneInfo("America/Los_Angeles")  # PST/PDT, same as the hub's time_local column
except Exception:
    sys.exit("Pacific time zone data not found: pip install tzdata")


def to_pacific(utc):
    # shortcut: naive local times, so the repeated hour when DST ends lands in one hour bin
    return utc.replace(tzinfo=dt.timezone.utc).astimezone(PACIFIC).replace(tzinfo=None)


def parse_time(s):
    # Hub CSVs mix padded and unpadded times, e.g. "2026-10-28 9:41:1"
    date, time = s.strip().split(" ")
    h, m, sec = (int(x) for x in time.split(":"))
    return dt.datetime.fromisoformat(date) + dt.timedelta(hours=h, minutes=m, seconds=sec)


def read_hub_csvs(folder):
    """{node: {(packet, minute): sample_time}} -- the key drops packets resent after a reboot."""
    readings = defaultdict(dict)
    for path in sorted(glob.glob(os.path.join(folder, "Hub*.csv"))):
        with open(path, newline="") as f:
            for row in list(csv.reader(f))[4:]:  # serial number, blank line, 2 header rows
                if len(row) < 5 or not row[1].isdigit() or not row[4].isdigit():
                    continue
                ts = parse_time(row[2])  # stays UTC so packet order survives the DST fall-back hour
                readings[int(row[1])][(int(row[4]), ts.replace(second=0))] = ts
    return readings


def received_and_missed(packets):
    """packets: [(sample_time, packet_number)] for one node -> (received_times, missed_times)."""
    packets = sorted(packets)
    received = [ts for ts, _ in packets]
    missed = []
    for (t1, p1), (t2, p2) in zip(packets, packets[1:]):
        n = p2 - p1 - 1  # a lower packet number means the node rebooted and restarted at 1
        if n > 0 and t2 - t1 < MAX_GAP:
            missed += [t1 + (t2 - t1) * i / (n + 1) for i in range(1, n + 1)]
    return received, missed


UPLOAD_FAILED = ("Failed to publish packet", "1 or more packets failed", "MQTT Retry limit exceeded",
                 "Failed to connect to broker", "Module or NetworkComponent not initialzed")


STOP_GAP = dt.timedelta(minutes=30)  # a log that ends this long before the next boot means the hub froze or lost power
GROUP_GAP = dt.timedelta(minutes=90)  # hub events this close together share one marker on the chart


def hub_log(folder):
    """From debug/output_*.log: per-hour upload successes/failures, hours with any log, and hub events.
    Events are (pacific_time, kind, text) with kind "watchdog", "reset", "lte" or "stop"."""
    stamp = re.compile(r"^\[(\d{4})-(\d{2})-(\d{2})T(\d{1,2}):(\d{1,2}):(\d{1,2})Z\]")
    ok, failed, alive, events, spans = set(), set(), set(), [], []
    paths = glob.glob(os.path.join(folder, "debug", "output_*.log"))
    for path in sorted(paths, key=lambda p: int(re.search(r"(\d+)\.log$", p).group(1))):
        first = last = None
        with open(path, errors="ignore") as f:
            for line in f:
                m = stamp.match(line)
                if not m:
                    continue
                t = to_pacific(dt.datetime(*(int(x) for x in m.groups())))
                first, last = first or t, t
                hour = floor_hour(t)
                alive.add(hour)
                if "Loom_MongoDB.cpp:publish" in line and "Data has been successfully sent" in line:
                    ok.add(hour)
                elif any(s in line for s in UPLOAD_FAILED):
                    failed.add(hour)
                if "Reset cause:" in line:
                    cause = line.split("Reset cause:")[1].strip()
                    events.append((t, "watchdog" if "Watchdog" in cause else "reset", cause.lower()))
                elif "Modem did not power on" in line:
                    events.append((t, "lte", "LTE fail"))
        if first:
            spans.append((first, last))
    for (_, end), (nxt, _) in zip(spans, spans[1:]):
        if nxt - end > STOP_GAP:
            events.append((end, "stop", "log stops"))
    return ok, failed, alive, events


def floor_hour(t):
    return t.replace(minute=0, second=0, microsecond=0)


def plot(folder, out):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.colors import ListedColormap
    from matplotlib.lines import Line2D
    from matplotlib.patches import Patch

    readings = read_hub_csvs(folder)
    if not readings:
        sys.exit(f"No Hub*.csv files with readings in {folder}")

    per_node = {}
    for node, recs in readings.items():
        got, miss = received_and_missed([(ts, key[0]) for key, ts in recs.items()])
        per_node[node] = ([to_pacific(t) for t in got], [to_pacific(t) for t in miss])

    start = floor_hour(min(min(r + m) for r, m in per_node.values()))
    end = floor_hour(max(max(r + m) for r, m in per_node.values()))
    hours = [start + i * HOUR for i in range(int((end - start) / HOUR) + 1)]
    idx = {h: i for i, h in enumerate(hours)}

    # 0 no readings, 1 all received, 2 some missed, 3 all missed
    nodes = sorted(per_node)
    grid, pct = [], []
    for node in nodes:
        got, miss = [0] * len(hours), [0] * len(hours)
        for t in per_node[node][0]:
            got[idx[floor_hour(t)]] += 1
        for t in per_node[node][1]:
            miss[idx[floor_hour(t)]] += 1
        grid.append([0 if not g and not m else 1 if not m else 3 if not g else 2 for g, m in zip(got, miss)])
        total = sum(got) + sum(miss)
        pct.append(f"{round(100 * sum(got) / total)}%  ({sum(got)}/{total})")

    ok, failed, alive, events = hub_log(folder)
    show_hub = bool(alive)

    fig, axes = plt.subplots(2 if show_hub else 1, 1, sharex=True, figsize=(14, 0.45 * len(nodes) + 3.2),
                             gridspec_kw={"height_ratios": [len(nodes), 1]} if show_hub else None)
    ax = axes[0] if show_hub else axes
    node_colors = ListedColormap(["#d3d1c7", "#0ca30c", "#fab219", "#d03b3b"])
    ax.imshow(grid, aspect="auto", interpolation="nearest", cmap=node_colors, vmin=0, vmax=3)
    ax.set_yticks(range(len(nodes)), [f"Node {n}" for n in nodes])
    for row, label in enumerate(pct):
        ax.text(len(hours) + 0.5, row, label, va="center", fontsize=9)
    ax.set_title("Dendrometer readings received by the hub, per node per hour (Pacific time)", pad=26)
    handles = [Patch(color="#0ca30c", label="All received"), Patch(color="#fab219", label="Some missed"),
               Patch(color="#d03b3b", label="All missed"), Patch(color="#d3d1c7", label="No readings")]

    if show_hub:
        # Uploads come in bursts, so an hour without one is idle, not a failure; a failure in the hour wins
        # 0 no log (frozen/off), 1 uploaded, 2 upload failed, 3 running with nothing to upload
        hub = [[0 if h not in alive else 2 if h in failed else 1 if h in ok else 3 for h in hours]]
        axes[1].imshow(hub, aspect="auto", interpolation="nearest",
                       cmap=ListedColormap(["#444441", "#0ca30c", "#d03b3b", "#d3d1c7"]), vmin=0, vmax=3)
        axes[1].set_yticks([0], ["Hub upload"])
        handles += [Patch(color="#0ca30c", label="Uploaded to MongoDB"), Patch(color="#d03b3b", label="Upload failed"),
                    Patch(color="#d3d1c7", label="Hub running, nothing to upload"),
                    Patch(color="#444441", label="No hub log (frozen or off)")]

        # Hub events as vertical lines. Events within GROUP_GAP share one numbered marker, explained under the chart
        style = {"watchdog": ("#d03b3b", "--"), "lte": ("#6250d6", "-."), "stop": ("#0b0b0b", "-"),
                 "reset": ("#5f5e5a", ":")}
        groups = []
        for t, kind, text in sorted(events):
            if not start <= t <= end + HOUR:
                continue
            for a in axes:
                a.axvline((t - start) / HOUR - 0.5, color=style[kind][0], linestyle=style[kind][1], linewidth=1.2)
            if groups and t - groups[-1][-1][0] <= GROUP_GAP:
                groups[-1].append((t, text))
            else:
                groups.append([(t, text)])
        notes = []
        for n, items in enumerate(groups, 1):
            ax.text((items[0][0] - start) / HOUR - 0.5, -0.6, str(n), ha="center", va="bottom", fontsize=8,
                    clip_on=False, bbox={"boxstyle": "round,pad=0.2", "fc": "white", "ec": "#5f5e5a", "lw": 0.6})
            counts = defaultdict(int)
            for _, text in items:
                counts[text] += 1
            when = f"{items[0][0]:%b %d %H:%M}" + (f"-{items[-1][0]:%H:%M}" if items[-1][0] != items[0][0] else "")
            notes.append(f"{n}  {when}: " + ", ".join(f"{t} x{c}" if c > 1 else t for t, c in counts.items()))
        if notes:
            fig.text(0.07, -0.16, "Hub events\n" + "\n".join(notes), va="top", fontsize=8.5, linespacing=1.5)
        handles += [Line2D([], [], color=c, linestyle=s, label=lbl) for lbl, (c, s) in
                    [("Watchdog reset", style["watchdog"]), ("Other reset (power, button)", style["reset"]),
                     ("LTE modem failed", style["lte"]), ("Hub log stops (froze/off)", style["stop"])]]
    fig.legend(handles=handles, loc="upper center", bbox_to_anchor=(0.5, 0.0), ncol=4, frameon=False, fontsize=9)

    days = [i for i, h in enumerate(hours) if h.hour == 0]
    (axes[-1] if show_hub else ax).set_xticks(days, [hours[i].strftime("%b %d") for i in days])
    fig.tight_layout()
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Wrote {out}: {len(nodes)} nodes, {hours[0]:%Y-%m-%d %H:00} to {hours[-1]:%Y-%m-%d %H:00} Pacific")


def selftest():
    t = dt.datetime(2026, 1, 1)
    q = dt.timedelta(minutes=15)
    got, miss = received_and_missed([(t, 1), (t + 3 * q, 4), (t + 4 * q, 1)])  # 2,3 missed, then a reboot
    assert len(got) == 3 and miss == [t + q, t + 2 * q], miss
    assert parse_time("2026-10-28 9:41:1") == dt.datetime(2026, 10, 28, 9, 41, 1)
    assert to_pacific(dt.datetime(2026, 10, 28, 22, 55)) == dt.datetime(2026, 10, 28, 15, 55)  # PDT, UTC-7
    assert to_pacific(dt.datetime(2026, 11, 2, 21, 55)) == dt.datetime(2026, 11, 2, 13, 55)  # PST, UTC-8
    print("selftest ok")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folder", nargs="?", default="E:/", help="hub SD card root (default E:/)")
    ap.add_argument("-o", "--out", default="node_connectivity.png")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    selftest() if a.selftest else plot(a.folder, a.out)
