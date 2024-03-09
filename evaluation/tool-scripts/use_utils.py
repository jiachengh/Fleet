#!/home/jiacheng/anaconda3/bin/python
import os
import time
import subprocess

app_list = {
    'com.twitter.android',
    'com.facebook.katana',
    'com.instagram.android',
    'org.telegram.messenger',
    'jp.naver.line.android',
    
    'com.google.android.youtube',
    'com.ss.android.ugc.aweme',
    'com.spotify.music',
    'tv.twitch.android.app',
    'com.wemesh.android',
    'sg.bigo.live',

    'com.amazon.mShop.android.shopping',
    'com.google.android.apps.maps',
    'com.android.chrome',
    'org.mozilla.firefox',
    'com.linkedin.android',

    'com.rovio.angrybirds',
    'com.king.candycrushsaga',
}


def start_twitter():
    raw_result = subprocess.run(['bash', './tool-scripts/start-twitter.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result

def use_app_twitter(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)



def start_facebook():
    raw_result = subprocess.run(['bash', './tool-scripts/start-facebook.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result

def use_app_facebook(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)


def start_instagram():
    raw_result = subprocess.run(['bash', './tool-scripts/start-instagram.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result

def use_app_instagram(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)


def start_telegram():
    raw_result = subprocess.run(['bash', './tool-scripts/start-telegram.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result

def use_app_telegram(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)


def start_line():
    raw_result = subprocess.run(['bash', './tool-scripts/start-line.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result

def use_app_line(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)


def start_youtube():
    raw_result = subprocess.run(['bash', './tool-scripts/start-youtube.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result

def use_app_youtube(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)


def start_tiktok():
    raw_result = subprocess.run(['bash', './tool-scripts/start-tiktok.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result


def use_app_tiktok(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)


def start_spotify():
    raw_result = subprocess.run(['bash', './tool-scripts/start-spotify.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result


def use_app_spotify(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)


def start_twitch():
    raw_result = subprocess.run(['bash', './tool-scripts/start-twitch.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result

def use_app_twitch(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)



def start_rave():
    raw_result = subprocess.run(['bash', './tool-scripts/start-rave.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result

def use_app_rave(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)


def start_bigolive():
    raw_result = subprocess.run(['bash', './tool-scripts/start-bigolive.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result

def use_app_bigolive(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)


def start_amazon():
    raw_result = subprocess.run(['bash', './tool-scripts/start-amazon.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result

def use_app_amazon(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)


def start_googlemaps():
    raw_result = subprocess.run(['bash', './tool-scripts/start-googlemaps.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result

def use_app_googlemaps(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)


def start_chrome():
    raw_result = subprocess.run(['bash', './tool-scripts/start-chrome.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result

def use_app_chrome(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)


def start_firefox():
    raw_result = subprocess.run(['bash', './tool-scripts/start-firefox.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result

def use_app_firefox(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)


def start_linkedin():
    raw_result = subprocess.run(['bash', './tool-scripts/start-linkedin.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result

def use_app_linkedin(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)


def start_angrybirds():
    raw_result = subprocess.run(['bash', './tool-scripts/start-angrybirds.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result

def use_app_angrybirds(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)


def start_candycrush():
    raw_result = subprocess.run(['bash', './tool-scripts/start-candycrush.sh'], stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    return result


def use_app_candycrush(duration, period=0):
    use_phone_always_swipe_up_fast(duration=duration/3, period=period)
    use_phone_always_swipe_up_and_down(duration=duration/3, period=period)
    use_phone_always_swipe_up(duration=duration/3, period=period)


def start_all_apps(duration=10):
    print("Begin start_all_apps()")
    result = start_twitter()
    print(result)
    use_app_twitter(duration)

    result = start_facebook()
    print(result)
    use_app_facebook(duration)
    
    result = start_instagram()
    print(result)
    use_app_instagram(duration)
    
    result = start_telegram()
    print(result)
    use_app_telegram(duration)
    
    result = start_line()
    print(result)
    use_app_line(duration)
    
    result = start_youtube()
    print(result)
    use_app_youtube(duration)
    
    result = start_tiktok()
    print(result)
    use_app_tiktok(duration)
    
    result = start_spotify()
    print(result)
    use_app_spotify(duration)
    
    result = start_twitch()
    print(result)
    use_app_twitch(duration)
    
    result = start_rave()
    print(result)
    use_app_rave(duration)
    
    result = start_bigolive()
    print(result)
    use_app_bigolive(duration)
    
    result = start_amazon()
    print(result)
    use_app_amazon(duration)
    
    result = start_googlemaps()
    print(result)
    use_app_googlemaps(duration)
    
    result = start_chrome()
    print(result)
    use_app_chrome(duration)
    
    result = start_firefox()
    print(result)
    use_app_firefox(duration)
    
    result = start_linkedin()
    print(result)
    use_app_linkedin(duration)

    print("End start_all_apps()")
    




def parsing_adb_am_result(result):
    # print(result)
    ss = result.split('\n')
    
    status, launch_state, wait_time = '', '', 0
    for s in ss:
        if s.startswith('Status:'):
            status = s
            status = status.split(':')
            status = status[1]
            status = status.strip()
        elif s.startswith('LaunchState:'):
            launch_state = s
            launch_state = launch_state.split(':')
            launch_state = launch_state[1]
            launch_state = launch_state.strip()
        elif s.startswith('WaitTime:'):
            wait_time = s
            wait_time = wait_time.split(':')
            wait_time = wait_time[1]
            wait_time = wait_time.strip()
            wait_time = int(wait_time)

    return status, launch_state, wait_time



def use_phone_always_swipe_up(duration=30, period=0):
    start = time.time()
    while True:
        if period > 0:
            time.sleep(period)
        os.system('tool-scripts/action-touchscreen-swipe-up.sh')
        if start + duration < time.time():
            break
        
def use_phone_always_swipe_up_fast(duration=30, period=0):
    start = time.time()
    while True:
        if period > 0:
            time.sleep(period)
        os.system('tool-scripts/action-touchscreen-swipe-up-fast.sh')
        if start + duration < time.time():
            break

def use_phone_always_swipe_up_and_down(duration=30, period=0):
    start = time.time()
    flag = True
    while True:
        if period > 0:
            time.sleep(period)
        if flag:
            os.system('tool-scripts/action-touchscreen-swipe-up-fast.sh')
            flag = False
        else:
            os.system('tool-scripts/action-touchscreen-swipe-down-fast.sh')
            flag = True
        if start + duration < time.time():
            break


def action_home():
    os.system('./tool-scripts/action-home.sh')


def kill_all_apps(check_cache_num=False):
    cmd0 = 'adb shell am force-stop %s'
    for app in app_list:
        cmd = cmd0 % app
        os.system(cmd)
    print('Killed all apps.')
    if check_cache_num:
        print('Cached APPs:', check_cached_apps())


def dumpsys_meminfo():
    cmd = 'adb shell dumpsys meminfo'
    raw_result = subprocess.run(cmd.split(), stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    # print(result)
    return result


def check_cached_apps():
    result = dumpsys_meminfo()
    res_cached_apps = set()
    for line in result.split('\n'):
        for x in line.split(' '):
            if x in app_list:
                res_cached_apps.add(x)
    return res_cached_apps

def check_cached_apps_pixel5():
    result = dumpsys_meminfo()
    res_cached_apps = set()
    for line in result.split('\n'):
        line = line.strip()
        for x in line.split(' '):
            if x in app_list:
                res_cached_apps.add(x)
    return res_cached_apps


if __name__ == '__main__':
    # kill_all_apps()
    cached_app = check_cached_apps_pixel5()
    print('cached_app=', cached_app)
    print('num=', len(cached_app))
