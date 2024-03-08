import os
import time
import subprocess
import sys

sys.path.append('tool-scripts')
import use_utils

FOREGROUND_DURATION = 10 # default 30
BACKGROUND_DURATION = 5 # default 30
REPEAT = 2

IF_KILL_ALL = True


result = use_utils.action_home()

if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_twitter()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)


if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_facebook()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)


if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_instagram()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)


if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_telegram()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)


if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_line()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)


if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_youtube()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)


if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_tiktok()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)


if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_spotify()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)


if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_twitch()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)


if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_rave()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)


if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_bigolive()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)


if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_amazon()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)


if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_googlemaps()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)

if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_chrome()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)


if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_firefox()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)


if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_linkedin()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)


if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_angrybirds()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)


if IF_KILL_ALL:
    use_utils.kill_all_apps()
for i in range(REPEAT):
    result = use_utils.start_candycrush()
    print(result)
    use_utils.use_phone_always_swipe_up(FOREGROUND_DURATION, 0)
    use_utils.action_home()
    time.sleep(BACKGROUND_DURATION)

    
print('Done!')