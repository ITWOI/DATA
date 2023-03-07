#!/usr/bin/python

'''
Record all SV accesses, the format of input should be like this:
line,SV,accessType,func

The second input is validated races, the format is as follows:
var,line,isrline

The third input is the original code.
'''

import sys
import os
from collections import OrderedDict


def parse_args(args):
    accessInfo = args[1]
    staticRes = args[2]
    sourceCode = args[3]
    return (accessInfo, staticRes, sourceCode)

def error(msg):
    print msg
    sys.exit(1)

def sortAccessInfo(accessInfo):
    accessInfoArr = OrderedDict()
    for line in accessInfo:
        arr = line.strip().split(",")
        arr[0] = int(arr[0])
        if accessInfoArr.has_key(arr[0]):
            accessInfoArr[arr[0]].append(arr[1:])
        else:
            tmp = []
            tmp.append(arr[1:])
            accessInfoArr[arr[0]] = tmp
    return accessInfoArr

def sortResInfo(staticRes, accessInfoArr):
    staticResArr = []
    ifAnother = True
    curFunc = None
    for line in staticRes:
        arr = line.split(" ")
        if(len(arr) == 1):
            ifAnother = True
            continue
        line = int(arr[1])
        if(ifAnother):
            curFunc = accessInfoArr[line][0][2]
            staticResArr.append(line)
        else:
            if curFunc != accessInfoArr[line][0][2]:
                curFunc = accessInfoArr[line][0][2]
                staticResArr.append(line)
    staticResArr.sort()
    print staticResArr
    return staticResArr


def analyzeCriticalSection(accessInfoArr, staticRes):
    # {beginLine:endLine}
    criticalSection = dict()
    curFun = ""
    print accessInfoArr
    staticResArr = sortResInfo(staticRes, accessInfoArr)

    index = 0
    while(index < len(staticResArr)):
        # {sv:accessType}
        valueDict = dict()
        curLine = staticResArr[index]
        if ifInCS(curLine, criticalSection):
            index = index + 1
            continue
        criticalSection[curLine] = curLine
        # find next read
        ifCont = False
        for line in accessInfoArr.keys():
            if ifCont == True:
                tmpInfo = accessInfoArr[line]
                ifBreak = False
                for ele in tmpInfo:
                    if ele[2] != curFun:
                        #criticalSection[curLine] = int(ele[0])
                        ifBreak = True
                    if ele[0] in valueDict:
                        if valueDict[ele[0]] == "write" and ele[1] == "read":
                            valueDict[ele[0]] = "read"
                            criticalSection[curLine] = int(line)
                    if ifAllRead(valueDict):
                        ifBreak = True
                if ifBreak:
                    break

            if line == curLine:
                ifCont = True
                for ele in accessInfoArr[line]:
                    valueDict[ele[0]] = ele[1]
                    curFun = ele[2]
                if ifAllRead(valueDict):
                    index = index + 1
                    break
    return criticalSection


def ifAllRead(valueDict):
    for val in valueDict:
        if valueDict[val] == "write":
            return False
    return True

def ifInCS(line, criticalSection):
    for ele in criticalSection:
        if line >= ele and line <= criticalSection[ele]:
            return True
    return False

def main():
    if len(sys.argv) != 4:
        error("usage %s <accessInfo> <StaticRes> <sourceCode>" % sys.argv[0])
    accessInfoFile, staticResFile, sourceCodeFile = parse_args(sys.argv)
    accessInfo = open(os.path.join(accessInfoFile), "r").readlines()
    staticRes = open(os.path.join(staticResFile), "r").readlines()
    sourceCode = open(os.path.join(sourceCodeFile), "r").readlines()
    accessInfoArr = sortAccessInfo(accessInfo)
    criticalSection = analyzeCriticalSection(accessInfoArr, staticRes)
    print criticalSection



if __name__ == "__main__":
    main()
