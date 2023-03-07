#!/usr/bin/python

import sys

def main():
    args = sys.argv
    if len(args) != 2:
        error("Usage %s <pdr_name> " % args[0])
    pdrFile = args[1]
    f = open(pdrFile, "r").readlines()
    for line in f:
        print line

if __name__ == '__main__':
    main()
    exit(1)
