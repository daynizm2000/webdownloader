import time
import os
from pathlib import Path
import sys
import shutil
import sys

Path("webdownloader_test").mkdir(exist_ok=True)

files = {"100": "100MB.zip", "10": "10MB.zip", "1": "1MB.zip"}
args = f"http://speedtest.tele2.net/{files['100']}" if not "--many-files" in sys.argv else f"http://speedtest.tele2.net/{files['1']} http://speedtest.tele2.net/{files['10']} http://speedtest.tele2.net/{files['100']} http://speedtest.tele2.net/{files['1']} http://speedtest.tele2.net/10MB.zip http://speedtest.tele2.net/100MB.zip http://speedtest.tele2.net/1MB.zip http://speedtest.tele2.net/{files['10']} http://speedtest.tele2.net/{files['100']} http://speedtest.tele2.net/{files['1']} http://speedtest.tele2.net/{files['10']} http://speedtest.tele2.net/{files['100']}"


if not Path("webd").exists():
    ret = os.system("make")

    if ret != 0:
        print("Failed to make project")
        sys.exit(ret)


os.chdir("webdownloader_test")


if not Path("webd").exists():
    shutil.copy2("../webd", "./webd")


start = time.perf_counter()
os.system(f"./webd {args}")
end = time.perf_counter()

print("Total time:", end - start)

print("Downloaded files:")

for file in Path(".").glob("*.zip"):
    if any(file.name.startswith(name.replace(".zip", "")) for name in files.values()):
        print(f"{file.name}: {file.stat().st_size}b ({(file.stat().st_size / (1024 * 1024)):.2f} MB)")

        if not "--not-delete-files" in sys.argv:
            file.unlink()


os.chdir("../")

if not "--not-delete-dir" in sys.argv:
    shutil.rmtree("webdownloader_test/")