import subprocess
import sys
import time

bins = sys.argv[1:3]
tests = sys.argv[3:]
print("test", *[f"{b}_time {b}_mem" for b in bins])
for t in tests:
    row = [t.split("/")[-1]]
    for b in bins:
        best = 1e9
        mem = 0
        for _ in range(5):
            with open(t) as f:
                st = time.perf_counter()
                r = subprocess.run(["/usr/bin/time", "-f", "%M", b], stdin=f,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
                best = min(best, time.perf_counter() - st)
                mem = int(r.stderr.split()[-1])
        row += [f"{best:.3f}", f"{mem / 1024:.1f}"]
    print(*row)
