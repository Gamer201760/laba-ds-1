import random
import sys

n = int(sys.argv[1])
mode = sys.argv[2] if len(sys.argv) > 2 else "mix"
r = random.Random(42)
words = []


def word():
    return "".join(r.choice("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") for _ in range(r.randint(1, 20)))


out = []
if mode == "mix":
    for _ in range(n):
        t = r.random()
        if t < 0.5 or not words:
            w = word()
            words.append(w)
            out.append(f"+ {w} {r.randint(0, 2**64 - 1)}")
        elif t < 0.8:
            out.append(r.choice(words))
        else:
            out.append(f"- {r.choice(words)}")
elif mode == "insert":
    for _ in range(n):
        out.append(f"+ {word()} {r.randint(0, 2**64 - 1)}")
elif mode == "sorted":
    for i in range(n):
        w = ""
        x = i
        for _ in range(5):
            w = chr(97 + x % 26) + w
            x //= 26
        out.append(f"+ {w} {i}")
elif mode == "saveload":
    for _ in range(n):
        out.append(f"+ {word()} {r.randint(0, 2**64 - 1)}")
    for _ in range(10):
        out.append("! Save dict.bin")
        out.append("! Load dict.bin")
print("\n".join(out))
