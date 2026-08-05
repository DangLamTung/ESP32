"""Vendor CelliesProjects/OpenStreetMap-esp32 into lib/ and drop library.json
so PlatformIO compiles ALL of its sources (the registry install only exposes a
single header, which breaks the sibling #includes)."""
import os, shutil, zipfile, urllib.request

URL = "https://github.com/CelliesProjects/OpenStreetMap-esp32/archive/refs/heads/main.zip"
ZIP = r"C:\Users\Tung\Downloads\osm-esp32-main.zip"
EXTRACT = r"C:\Users\Tung\Downloads\osm-esp32-extract"
LIB = r"C:\Users\Tung\Documents\ESP32\ESP32\osm_touch\lib\OpenStreetMap-esp32"

os.makedirs(EXTRACT, exist_ok=True)
if not os.path.exists(ZIP) or os.path.getsize(ZIP) < 10000:
    print("downloading...")
    urllib.request.urlretrieve(URL, ZIP)
print("extracting...")
with zipfile.ZipFile(ZIP) as z:
    z.extractall(EXTRACT)

src = None
for d in os.listdir(EXTRACT):
    cand = os.path.join(EXTRACT, d)
    if os.path.isdir(cand) and "OpenStreetMap" in d:
        src = cand
        break
if not src:
    print("ERROR: could not find extracted repo dir")
    sys.exit(1)

if os.path.exists(LIB):
    shutil.rmtree(LIB)
shutil.copytree(src, LIB)

j = os.path.join(LIB, "library.json")
if os.path.exists(j):
    os.remove(j)
    print("removed library.json (so all sources compile)")

print("vendored to", LIB)
print(sorted(os.listdir(LIB)))
print(sorted(os.listdir(os.path.join(LIB, "src")))[:40])
