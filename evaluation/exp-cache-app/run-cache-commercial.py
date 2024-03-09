import os
import time
import subprocess
import sys

sys.path.append('tool-scripts')

import use_utils

USE_DURATION = 30  # default 30
IF_KILL_ALL = True
MANUAL_INTERACTION = False # default True

if IF_KILL_ALL:
    use_utils.kill_all_apps()

use_utils.action_home()

if IF_KILL_ALL:
    use_utils.kill_all_apps()

cached_number = []

for i in range(2):    
    result = use_utils.start_twitter()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_twitter(USE_DURATION)
        else:
            time.sleep(USE_DURATION)


    
    result = use_utils.start_facebook()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_facebook(USE_DURATION)
        else:
            time.sleep(USE_DURATION)



    result = use_utils.start_instagram()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_instagram(USE_DURATION)
        else:
            time.sleep(USE_DURATION)




    result = use_utils.start_telegram()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_telegram(USE_DURATION)
        else:
            time.sleep(USE_DURATION)



    result = use_utils.start_line()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_line(USE_DURATION)
        else:
            time.sleep(USE_DURATION)



    result = use_utils.start_youtube()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_youtube(USE_DURATION)
        else:
            time.sleep(USE_DURATION)




    result = use_utils.start_tiktok()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_tiktok(USE_DURATION)
        else:
            time.sleep(USE_DURATION)



    result = use_utils.start_spotify()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_spotify(USE_DURATION)
        else:
            time.sleep(USE_DURATION)




    result = use_utils.start_twitch()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_twitch(USE_DURATION)
        else:
            time.sleep(USE_DURATION)



    result = use_utils.start_rave()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_rave(USE_DURATION)
        else:
            time.sleep(USE_DURATION)



    result = use_utils.start_bigolive()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_bigolive(USE_DURATION)
        else:
            time.sleep(USE_DURATION)



    result = use_utils.start_amazon()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_amazon(USE_DURATION)
        else:
            time.sleep(USE_DURATION)



    result = use_utils.start_googlemaps()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_googlemaps(USE_DURATION)
        else:
            time.sleep(USE_DURATION)



    result = use_utils.start_chrome()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_chrome(USE_DURATION)
        else:
            time.sleep(USE_DURATION)



    result = use_utils.start_firefox()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_firefox(USE_DURATION)
        else:
            time.sleep(USE_DURATION)


    

    result = use_utils.start_linkedin()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_linkedin(USE_DURATION)
        else:
            time.sleep(USE_DURATION)




    result = use_utils.start_angrybirds()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_angrybirds(USE_DURATION)
        else:
            time.sleep(USE_DURATION)




    result = use_utils.start_candycrush()
    print(result)
    cached_apps = use_utils.check_cached_apps()
    print('CachingNUM= %d, Cached APPs: %s\n' % (len(cached_apps), cached_apps))
    cached_number.append(len(cached_apps))
    
    status, launch_state, wait_time = use_utils.parsing_adb_am_result(result)
    if status == 'ok':
        if not MANUAL_INTERACTION:
            use_utils.use_app_candycrush(USE_DURATION)
        else:
            time.sleep(USE_DURATION)


print()
print('cached_numbers=', cached_number)
