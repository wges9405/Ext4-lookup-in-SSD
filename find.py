import os
import numpy as np
import random as rd
# --------------------------------------------------------------------------------------- #
# Linux command:
#   Free pagecache & dentries & inodes - $ sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"
#   Linux Find - $ find femu/linux-5.4/fs/ext4/namei.c
# --------------------------------------------------------------------------------------- # 

# --------------------------------------------------------------------------------------------------------- # 
# Pre condition:
#   1. Linux v5.4.0, FEMU
#   2. create a empty file at ./
#       - $ sudo mkdir femu
#   3. mkfs simulated SSD (Ext4) and mount it to ./femu 
#       - $ sudo mount /dev/nvme0n1 femu
#   4. download linux kernel v5.4.0 in femu  (can download any file, we just need to do lookup to the file)
#       - $ cd femu
#       - $ wget https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.4.tar.xz
#       - $ unxz -v linux-5.4.tar.xz
#       - $ tar xvf linux-5.4.tar
#   5. prepare file list of ./femu
#       - $ cd .. (back to ./)
#       - $ sudo ls -R ./femu > filelist.txt
#       - $ touch fileorder.txt  (fileorder for recording order of random lookup)
#   6. put this .py into ./
#   7. do experiment
# --------------------------------------------------------------------------------------------------------- # 


# Clean all cache before starting
os.system('sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"')

# flag for cleaning cache 
clean_cache = True

# lookup times
times = 100

fileorder = []
free_cache_cmd = 'sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"'
find_cmd = ''

# read filelist or fileorder
if clean_cache:
    file = np.genfromtxt('./filelist.txt', dtype='unicode')
    prefix = ''
    filelist = []
    for idx, str in enumerate(file):
        if ':' in str:
            prefix = str[:-1]
        elif '.' in str:
            filelist.append(prefix + '/' + str)
else:
    filelist = np.genfromtxt('./fileorder.txt', dtype='unicode')


# ---------------------------------------------------------------------------------- # 
# 1. random lookup file in filelist and record into fileorder.txt with cleaned cache
# 2. lookup file in fileorder in order with uncleaned cache
# ---------------------------------------------------------------------------------- # 
len = len(filelist)
while times > 0:
    if clean_cache:
        find_cmd = 'find ' + filelist[rd.randint(1, len) - 1]
        fileorder.append(find_cmd[5:])
    else:
        find_cmd = 'find ' + filelist[len - times]
        
    if clean_cache: os.system(free_cache_cmd)
    os.system(find_cmd)
    
    times = times - 1
if clean_cache:
    np.savetxt('./fileorder.txt', np.array(fileorder), fmt="%s")