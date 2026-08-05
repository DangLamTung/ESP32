"""Print the last N lines of a PowerShell-redirected log (handles utf-16/utf-8)."""
import sys

path = sys.argv[1] if len(sys.argv) > 1 else "build3.log"
n = int(sys.argv[2]) if len(sys.argv) > 2 else 60

data = open(path, "rb").read()
text = None
for enc in ("utf-16", "utf-8", "latin-1"):
    try:
        text = data.decode(enc)
        break
    except Exception:
        continue
if text is None:
    text = data.decode("latin-1", errors="replace")

lines = [ln for ln in text.splitlines() if ln.strip()]
print("\n".join(lines[-n:]))
