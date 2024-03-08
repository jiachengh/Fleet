#!/home/jiacheng/anaconda3/bin/python
import use_utils

if __name__ == '__main__':
    res_cached_apps = use_utils.check_cached_apps()
    print('CacheNUM= %d, CachedAPPs= %s' % (len(res_cached_apps), res_cached_apps))
