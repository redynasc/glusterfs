import sys
import os
import time
import requests
import threading
import random
import signal

is_sigint_up = False

#def sigint_handler(signum, frame):
#      is_sigint_up = True
#      print 'catched interrupt signal!'

#signal.signal(signal.SIGINT, sigint_handler)
#signal.signal(signal.SIGHUP, sigint_handler)
#signal.signal(signal.SIGTERM, sigint_handler)

class myFile():
    def __init__(self, name, sha1sum):
        self._full_name = name
        self._sha1sum = sha1sum

test_files=[]

def initTestFiles(file, out):
    ff = open(file,"r")
    while 1:
        line = ff.readline().split("\n")[0]
        if not line:
            break
        ret = line.split(' ', 1 )
        #print "apend %s %s"%(ret[1], ret[0])
        out.append(myFile(ret[1], ret[0]))

class readthread(threading.Thread):
    def __init__(self, name):
        threading.Thread.__init__(self)
        self.name = name

    def read(self, f):
        total = len(test_files)
        cmd = "sha1sum -b %s |awk '{print $1}'" %(f._full_name)
        #print cmd
        r = os.popen(cmd)
        file_sha = r.readline()
        file_sha = file_sha.replace("\n","")
        #print file_sha
        r.close()
        if f._sha1sum != file_sha:
            print "%s hash error %s!=%s  cmd=%s"%(f._full_name, file_sha, f._sha1sum, cmd)
        #else:
        #    print "%s hash success %s==%s"%(f._full_name, file_sha, f._sha1sum) 

    def run(self):
        total = len(test_files) 
        while 1:
            if is_sigint_up:
                return
            sel = random.randint(0,total-1)
            self.read(test_files[sel])
            time.sleep(0.005)
            #exit()

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print "python fs-cache-base-test.py sha1-filelist threads"
        exit(1)
    sha1_filelist=sys.argv[1]
    threads=int(sys.argv[2])

    print "start %s" %(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()) )

    initTestFiles(sha1_filelist, test_files)
    print "initTestFiles count=%d" % (len(test_files))
    threads_inst = []
    for i in range(threads):
        threads_inst.append(readthread(str(i)))

    time.sleep(1)
    for t in threads_inst:
        t.start()

    for t in threads_inst:
        t.join()
    print "end %s" %(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()) )




