#!/home/jiacheng/anaconda3/bin/python
import os
import time
import subprocess
import sys

import use_utils


def process_dumpinfo(info):
    ss = info.split('\n')
    is_data = False
    res = []
    for line in ss:
        line = line.strip()
        # print(line)

        if is_data:
            ts = line.split()
            if len(ts) != 4:
                is_data = False
                continue
            t_draw = float(ts[0])
            t_prepare = float(ts[1])
            t_process = float(ts[2])
            t_execute = float(ts[3])
            res.append((t_draw, t_prepare, t_process, t_execute))

        if line.startswith('Draw'):
            is_data = True

    return res


def get_frames(package_name):
    # cmd = 'adb shell dumpsys gfxinfo %s framestats reset'
    cmd = 'adb shell dumpsys gfxinfo %s reset'
    cmd = cmd % package_name
    raw_result = subprocess.run(cmd.split(), stdout=subprocess.PIPE)
    result = str(raw_result.stdout, 'utf-8')
    res = process_dumpinfo(result)
    return res


if __name__ == '__main__':
    input = sys.argv
    package_name = input[1]
    idx = 0
    while True:
        res = get_frames(package_name=package_name)
        for r in res:
            idx += 1
            print('idx= %d total= %.2f draw= %.2f prepare= %.2f execute= %.2f' % (idx, sum(r), r[0], r[1], r[2]))
        time.sleep(0.5)

