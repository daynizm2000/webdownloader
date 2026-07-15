import time
import os
from pathlib import Path
import sys

args = "http://speedtest.tele2.net/100MB.zip" # "http://speedtest.tele2.net/1MB.zip http://speedtest.tele2.net/10MB.zip http://speedtest.tele2.net/100MB.zip http://speedtest.tele2.net/1MB.zip http://speedtest.tele2.net/10MB.zip http://speedtest.tele2.net/100MB.zip http://speedtest.tele2.net/1MB.zip http://speedtest.tele2.net/10MB.zip http://speedtest.tele2.net/100MB.zip http://speedtest.tele2.net/1MB.zip http://speedtest.tele2.net/10MB.zip http://speedtest.tele2.net/100MB.zip"

if not Path("webd").exists():
        ret = os.system("make")

        if ret != 0:
            print("Failed to make project")
            sys.exit(ret)


start = time.time()
os.system(f"./webd {args}")
end = time.time()

print("Total time:", end - start)