#!/usr/bin/python

import os
import sys
import time

script_dir = os.path.dirname(sys.argv[0])

info_file = "Info.py"


def error(msg):
    print msg
    sys.exit(1)

def parse_args(args):
    if len(args) != 9:
        error("Usage %s <PuT> <loc> <isr> <loc> <svName> <irq> <symbolName> <logName>"
              % args[0])
    PuT = args[1]
    putLoc = args[2]
    isr = args[3]
    isrLoc = args[4]
    svName = args[5]
    irq = args[6]
    symbolName = args[7]
    logName = args[8]
    return (PuT, putLoc, isr, isrLoc, svName, irq, symbolName, logName)


def read_lic_template():
    return open(os.path.join(script_dir, "Info.txt"), "r").readlines()

def gen_license(template, PuT, isr, svName, irq, symbolName, logName):
    tmp = template[:] # copy
    for i in range(len(tmp)):
        tmp[i] = tmp[i].replace("_MAGIC_ENABLE_", "False")
        tmp[i] = tmp[i].replace("_PUT_", "\""+PuT+"\"")
        tmp[i] = tmp[i].replace("_ISR_", "\""+isr+"\"")
        tmp[i] = tmp[i].replace("_SVADDR_", "None")
        tmp[i] = tmp[i].replace("_SV_NAME_", "\""+svName+"\"")
        tmp[i] = tmp[i].replace("_IRQ_", irq)
        tmp[i] = tmp[i].replace("_SYMBOL_NAME_", "\""+symbolName+"\"")
        tmp[i] = tmp[i].replace("_MODULE_NAME_", "\"\"")
        tmp[i] = tmp[i].replace("_LOG_", logName)

    return tmp

def write_Info(info, write_path):
    with open(write_path, "wb") as f:
        for l in info:
            f.write(l)

def main():
    #currently, only one PuT is acceptable
    PuT, putLoc, isr, isrLoc, svName, irq, symbolName, logName = parse_args(sys.argv)
    Info_template = read_lic_template()
    Info = gen_license(Info_template, PuT, isr, svName, irq, symbolName,\
                       logName)
    write_path = os.path.join(script_dir,info_file)
    if os.path.exists(write_path):
        error("File %s already exists" % info_file)
    write_Info(Info,write_path)


if __name__ == "__main__":
    main()
