# please use "adb logcat | grep jiacheng | tee ~/gc-working-set-twitch-fleet(android).log"
# during running of this script

import os
import time
import subprocess
import sys

sys.path.append('tool-scripts')

import use_utils


FOREGROUND_DURATION_1 = 180
BACKGROUND_DURATION = 300
FOREGROUND_DURATION_2 = 120
SLEEP_UNIT = 2

MANUAL_INTERACTION = False
IF_KILL_ALL = False

if IF_KILL_ALL:
    use_utils.kill_all_apps()

use_utils.action_home()

if IF_KILL_ALL:
    use_utils.kill_all_apps()

# Please modify the follwoing line to change the tested app
use_utils.start_twitch()
# use_utils.start_tiktok()
# use_utils.start_facebook()
# use_utils.start_line()

start_time = time.time()

# use the app in the foreground
while time.time() - start_time < FOREGROUND_DURATION_1:
    if not MANUAL_INTERACTION:
        use_utils.use_app_twitch(SLEEP_UNIT) # we should manually use the phone during the time
    else:
        time.sleep(SLEEP_UNIT)

# switch the app to the background
use_utils.action_home()

while time.time() - start_time < FOREGROUND_DURATION_1 + BACKGROUND_DURATION:
    time.sleep(SLEEP_UNIT)


# use the app in the foreground
use_utils.start_twitch()
while time.time() - start_time < FOREGROUND_DURATION_1 + BACKGROUND_DURATION + FOREGROUND_DURATION_2:
    if not MANUAL_INTERACTION:
        use_utils.use_app_twitch(SLEEP_UNIT)
    else:
        time.sleep(SLEEP_UNIT)

use_utils.kill_all_apps()