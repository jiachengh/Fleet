import os
import time
import subprocess
import sys

sys.path.append('tool-scripts')

import use_utils

USE_DURATION = 6 # default 6 (6*5=30 seconds)
REPEAT_TIME = 3  # default 24

# Record the hot launch times
hot_launch_time_twitter = []
hot_launch_time_facebook = []
hot_launch_time_instagram = []
hot_launch_time_telegram = []
hot_launch_time_line = []

hot_launch_time_youtube = []
hot_launch_time_tiktok = []
hot_launch_time_spotify = []
hot_launch_time_twitch = []
hot_launch_time_rave = []
hot_launch_time_bigolive = []

hot_launch_time_amazon = []
hot_launch_time_googlemaps = []
hot_launch_time_chrome = []
hot_launch_time_firefox = []
hot_launch_time_linkedin = []

hot_launch_time_angrybirds = []
hot_launch_time_candycrush = []

# Record the cold launch times
cold_launch_time_twitter = []
cold_launch_time_facebook = []
cold_launch_time_instagram = []
cold_launch_time_telegram = []
cold_launch_time_line = []

cold_launch_time_youtube = []
cold_launch_time_tiktok = []
cold_launch_time_spotify = []
cold_launch_time_twitch = []
cold_launch_time_rave = []
cold_launch_time_bigolive = []

cold_launch_time_amazon = []
cold_launch_time_googlemaps = []
cold_launch_time_chrome = []
cold_launch_time_firefox = []
cold_launch_time_linkedin = []

cold_launch_time_angrybirds = []
cold_launch_time_candycrush = []


# Other (warm,...)
other_launch_time_twitter = []
other_launch_time_facebook = []
other_launch_time_instagram = []
other_launch_time_telegram = []
other_launch_time_line = []

other_launch_time_youtube = []
other_launch_time_tiktok = []
other_launch_time_spotify = []
other_launch_time_twitch = []
other_launch_time_rave = []
other_launch_time_bigolive = []

other_launch_time_amazon = []
other_launch_time_googlemaps = []
other_launch_time_chrome = []
other_launch_time_firefox = []
other_launch_time_linkedin = []

other_launch_time_angrybirds = []
other_launch_time_candycrush = []


use_utils.kill_all_apps()

use_utils.start_all_apps()

for i in range(REPEAT_TIME):    
    result = use_utils.start_telegram()
    print(result)
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if launch_state == 'HOT':
            hot_launch_time_telegram.append(wait_time)
        elif launch_state == 'COLD':
            cold_launch_time_telegram.append(wait_time)
        else:
            other_launch_time_telegram.append(wait_time)
    use_utils.use_app_telegram(USE_DURATION)


    result = use_utils.start_tiktok()
    print(result)
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if launch_state == 'HOT':
            hot_launch_time_tiktok.append(wait_time)
        elif launch_state == 'COLD':
            cold_launch_time_tiktok.append(wait_time)
        else:
            other_launch_time_tiktok.append(wait_time)
    use_utils.use_app_tiktok(USE_DURATION)


    result = use_utils.start_rave()
    print(result)
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if launch_state == 'HOT':
            hot_launch_time_rave.append(wait_time)
        elif launch_state == 'COLD':
            cold_launch_time_rave.append(wait_time)
        else:
            other_launch_time_rave.append(wait_time)
    use_utils.use_app_rave(USE_DURATION)

    result = use_utils.start_bigolive()
    print(result)
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if launch_state == 'HOT':
            hot_launch_time_bigolive.append(wait_time)
        elif launch_state == 'COLD':
            cold_launch_time_bigolive.append(wait_time)
        else:
            other_launch_time_bigolive.append(wait_time)
    use_utils.use_app_bigolive(USE_DURATION)



    result = use_utils.start_linkedin()
    print(result)
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if launch_state == 'HOT':
            hot_launch_time_linkedin.append(wait_time)
        elif launch_state == 'COLD':
            cold_launch_time_linkedin.append(wait_time)
        else:
            other_launch_time_linkedin.append(wait_time)
    use_utils.use_app_linkedin(USE_DURATION)


    result = use_utils.start_candycrush()
    print(result)
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if launch_state == 'HOT':
            hot_launch_time_candycrush.append(wait_time)
        elif launch_state == 'COLD':
            cold_launch_time_candycrush.append(wait_time)
        else:
            other_launch_time_candycrush.append(wait_time)
    use_utils.use_app_candycrush(USE_DURATION * 2)


# Output hot launch times
print('hot_launch_time_twitter=', hot_launch_time_twitter)
print('hot_launch_time_facebook=', hot_launch_time_facebook)
print('hot_launch_time_instagram=', hot_launch_time_instagram)
print('hot_launch_time_telegram=', hot_launch_time_telegram)
print('hot_launch_time_line=', hot_launch_time_line)

print('hot_launch_time_youtube=', hot_launch_time_youtube)
print('hot_launch_time_tiktok=', hot_launch_time_tiktok)
print('hot_launch_time_spotify=', hot_launch_time_spotify)
print('hot_launch_time_twitch=', hot_launch_time_twitch)
print('hot_launch_time_rave=', hot_launch_time_rave)
print('hot_launch_time_bigolive=', hot_launch_time_bigolive)

print('hot_launch_time_amazon=', hot_launch_time_amazon)
print('hot_launch_time_googlemaps=', hot_launch_time_googlemaps)
print('hot_launch_time_chrome=', hot_launch_time_chrome)
print('hot_launch_time_firefox=', hot_launch_time_firefox)
print('hot_launch_time_linkedin=', hot_launch_time_linkedin)

print('hot_launch_time_angrybirds=', hot_launch_time_angrybirds)
print('hot_launch_time_candycrush=', hot_launch_time_candycrush)

# Output cold launch times
print()
print('cold_launch_time_twitter=', cold_launch_time_twitter)
print('cold_launch_time_facebook=', cold_launch_time_facebook)
print('cold_launch_time_instagram=', cold_launch_time_instagram)
print('cold_launch_time_telegram=', cold_launch_time_telegram)
print('cold_launch_time_line=', cold_launch_time_line)

print('cold_launch_time_youtube=', cold_launch_time_youtube)
print('cold_launch_time_tiktok=', cold_launch_time_tiktok)
print('cold_launch_time_spotify=', cold_launch_time_spotify)
print('cold_launch_time_twitch=', cold_launch_time_twitch)
print('cold_launch_time_rave=', cold_launch_time_rave)
print('cold_launch_time_bigolive=', cold_launch_time_bigolive)

print('cold_launch_time_amazon=', cold_launch_time_amazon)
print('cold_launch_time_googlemaps=', cold_launch_time_googlemaps)
print('cold_launch_time_chrome=', cold_launch_time_chrome)
print('cold_launch_time_firefox=', cold_launch_time_firefox)
print('cold_launch_time_linkedin=', cold_launch_time_linkedin)

print('cold_launch_time_angrybirds=', cold_launch_time_angrybirds)
print('cold_launch_time_candycrush=', cold_launch_time_candycrush)

