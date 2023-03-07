#!/usr/bin/python

import sys
import commands
import os
import shutil

SIMICS_PATH = ""

def error(msg):
    print msg
    sys.exit(1)

def main():
    if len(sys.argv) != 6:
        error("Usage %s <pdr_file_name> <checkpoint_name> <Symbol_file> <log_file> <simics_path>" % sys.args[0])
    stat, output = commands.getstatusoutput('python ./scripts/analysePDR.py \
                                             %s' % sys.argv[1])
    print stat,output
    output = output.split('\n')
    i = 1
    SIMICS_PATH = sys.argv[5]
    logFileName = sys.argv[4] + "_" + str(i) + ".log"
    simicsCommand = SIMICS_PATH + "/simics -c '%s' -p 'conf.py' -e 'c'" % sys.argv[2]
    oldWorkDir = os.getcwd()
    for line in output:
        if line == '':
            continue
        print "python ./scripts/gen-Info.py %s %s %s" % (line, sys.argv[3], logFileName)
        simStat, simOutput = commands.getstatusoutput("python ./scripts/gen-Info.py %s %s %s" % (line, sys.argv[3], logFileName))
        print simStat, simOutput
        if not os.path.exists("./scripts/Info.py"):
            error("file not exit")
        if os.path.exists(SIMICS_PATH+"/Info.py"):
            os.remove(SIMICS_PATH+"/Info.py")
        shutil.move("./scripts/Info.py",SIMICS_PATH+"/Info.py")
        os.chdir(SIMICS_PATH)
        os.system(simicsCommand)
        os.chdir(oldWorkDir)
        i = i + 1
        logFileName = sys.argv[4] + "_" + str(i) + ".log"

if __name__ == "__main__":
    main()
