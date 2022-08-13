import pandas as pd
import numpy as np
import os
import subprocess
from datetime import date,datetime
dd=datetime.now().strftime("%Y%m%d")
dataDir="/mnt/ashish/crypto/ccapi_datasets/" + dd
os.makedirs(dataDir,exist_ok=True)
os.chdir(dataDir)
subprocess.call(["/home/ashish/git/ccapi/cmake-build-release/example/src/market_data_advanced_subscription/market_data_advanced_subscription"],shell=True)


