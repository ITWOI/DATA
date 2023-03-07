#!/usr/bin/python

import sys
import os
import re, copy

def parse_args(args):
    PuT = args[1]
    putLoc = args[2]
    isr = args[3]
    isrLoc = args[4]
    svName = args[5]
    irq = args[6]
    symbolName = args[7]
    logName = args[8]
    source = args[9]
    return (PuT, putLoc, isr, isrLoc, svName, irq, symbolName, logName, source)

def error(msg):
    print msg
    sys.exit(1)

def fixByIRQ(codes, irq, putLoc, isrLoc):
    tmp = codes[:]
    tmpLoc = (putLoc if putLoc > isrLoc else isrLoc)
    tmp[tmpLoc] = "disable_irq(%s);\n %s enable_irq(%s);\n" % (irq, tmp[tmpLoc],irq)
    tmpLoc = (putLoc if putLoc < isrLoc else isrLoc)
    tmp[tmpLoc] = "disable_irq(%s);\n %s enable_irq(%s);\n" % (irq, tmp[tmpLoc],irq)
    return tmp

def locFun(codes, loc):
    upLine = loc - 1
    lowLine = loc + 1
    count = 0
    while True:
        if codes[upLine].find("{") != -1:
            count += 1
            if re.match( r"(.*) (.*?)\(.*\)", codes[upLine-1]) != None:
                if codes[upLine].startswith(" "):
                    upLine -= 1
                    continue
                break
        if codes[upLine].find("}") != -1:
            count -= 1
        upLine -= 1
    while True:
        if codes[lowLine].find("}") != -1:
            count -= 1
        if count == 0:
            break
        lowLine += 1
    return (upLine, lowLine)

def locLock(codes, ul, ll):
    lockInfo = []
    for line in range(ul, ll):
        pos = codes[line].find("spin_lock")
        if pos != -1:
            ind = 0
        else:
            pos = codes[line].find("spin_unlock")
            if pos != -1:
                ind = 1
        if pos != -1:
            pos = codes[line].find("&", pos)
            endPos = codes[line].find(")", pos)
            lock = codes[line][pos+1:endPos]
            tmp=[lock, ind, line]
            lockInfo.append(tmp)
    return lockInfo

def decideLockType(codes, putLoc, isrLoc, putLockInfo, isrLockInfo):
    lockName = "newlock"
    putLockInfoWithPut = copy.deepcopy(putLockInfo)
    isrLockInfoWithisr = copy.deepcopy(isrLockInfo)
    isrLockInfoLen = len(isrLockInfo)
    putLockInfoLen = len(putLockInfo)
    isrLockInd = [0,0]
    putLockInd = [0,0]
    for i in range(len(putLockInfoWithPut)):
        if putLockInfoWithPut[i][2] > putLoc:
            putLockInfoWithPut.insert(i, [lockName, 1, 0])
            putLockInfoWithPut.insert(i, [lockName, 0, 0])
            putLockInd[0] = i-1
            putLockInd[1] = i
            break
    if len(putLockInfoWithPut) == putLockInfoLen:
        putLockInfoWithPut.append([lockName, 0, 0])
        putLockInfoWithPut.append([lockName, 1, 0])
        putLockInd[0] = putLockInfoLen - 1
        putLockInd[1] = -1
    for i in range(len(isrLockInfoWithisr)):
        if isrLockInfoWithisr[i][2] > isrLoc:
            isrLockInfoWithisr.insert(i, [lockName, 1, 0])
            isrLockInfoWithisr.insert(i, [lockName, 0, 0])
            isrLockInd[0] = i-1
            isrLockInd[1] = i
            break
    if len(isrLockInfoWithisr) == isrLockInfoLen:
        isrLockInfoWithisr.append([lockName, 0, 0])
        isrLockInfoWithisr.append([lockName, 1, 0])
        isrLockInd[0] = isrLockInfoLen - 1
        isrLockInd[1] = -1

    #comapre whether the order of lock is consistent.
    shareSet = set()
    for i in range(len(putLockInfoWithPut)):
        shareSet.add(putLockInfoWithPut[i][0])
    isrSet = set()
    for i in range(len(isrLockInfoWithisr)):
        isrSet.add(isrLockInfoWithisr[i][0])
    shareSet.intersection_update(isrSet)
    isrLockInfoWithisr = [x for x in isrLockInfoWithisr if x[0] in shareSet]
    putLockInfoWithPut = [x for x in putLockInfoWithPut if x[0] in shareSet]
    if len(isrLockInfoWithisr) == 0 or len(putLockInfoWithPut) == 0:
        return (putLoc, isrLoc, lockName)
    putInd = 0
    for i in range(len(isrLockInfoWithisr)):
        if isrLockInfoWithisr[i][0] == putLockInfoWithPut[putInd][0]:
            putInd += 1
        else:
            # to see if we can enlarge critical section
            shareSet = set()
            for ind in isrLockInd:
                if ind != -1:
                    shareSet.add(isrLockInfo[ind][0])
            tmpSet = set()
            for ind in putLockInd:
                if ind != -1:
                    tmpSet.add(putLockInfo[ind][0])
            shareSet.intersection_update(tmpSet)
            for i in shareSet:
                return (putLoc, isrLoc, i)
            return (0, 0, "")
    return (putLoc, isrLoc, lockName)

def doFixByLock(codes, lockName, putLoc, isrLoc):
    tmp = codes[:]
    ifFind = False;
    for index in range(len(codes)):
        if tmp[index].find("#include") != -1:
            ifFind = True;
        if tmp[index].find("#include") == -1 and ifFind == True:
            tmp[index] = "#include <asm/spinlock.h>\nspinlock_t %s;\n %s" % (lockName, tmp[index])
            break

    tmpLoc = (putLoc if putLoc > isrLoc else isrLoc)
    tmp[tmpLoc] = "spin_lock(&%s);\n %s spin_unlock(&%s);\n" % (lockName, tmp[tmpLoc], lockName)
    tmpLoc = (putLoc if putLoc < isrLoc else isrLoc)
    tmp[tmpLoc] = "spin_lock(&%s);\n %s spin_unlock(&%s);\n" % (lockName, tmp[tmpLoc], lockName)
    return tmp

def fixByMergeLock(codes, putLoc, isrLoc):
    return codes

def fixByLock(codes, putLoc, isrLoc):
    putUploc, putLowloc = locFun(codes, putLoc)
    isrUploc, isrLowloc = locFun(codes, isrLoc)
    funLocInfo = [putUploc, putLowloc, isrUploc, isrLowloc]
    putLockInfo = locLock(codes, putUploc, putLowloc)
    isrLockInfo = locLock(codes, isrUploc, isrLowloc)
    res = decideLockType(codes, putLoc, isrLoc, putLockInfo, isrLockInfo)
    if res and res[0] != 0:
        return doFixByLock(codes, res[2], putLoc, isrLoc)
    elif res and res[0] == 0:
        return fixByMergeLock(codes, putLoc, isrLoc)


def fixBug(codes, irq, putLoc, isrLoc, fixStr):
    putLoc = int(putLoc) - 1
    isrLoc = int(isrLoc) - 1
    if fixStr=="irq":
        return fixByIRQ(codes, irq, putLoc, isrLoc)
    elif fixStr == "lock":
        return fixByLock(codes, putLoc, isrLoc)

def writeCodes(codes, write_path):
    with open(write_path, "wb") as f:
        for l in codes:
            f.write(l)

def main():
    if len(sys.argv) != 10:
        error("usage %s <PuT> <loc> <isr> <loc> <fix strategy> <irq> <symbolName> <logName> <sourceName>" % sys.argv[0])
    PuT, putLoc, isr, isrLoc, fixStra, irq, symbolName, logName, sourceCode= parse_args(sys.argv)
    codes = open(os.path.join(sourceCode), "r").readlines()
    newCodes = fixBug(codes, irq, putLoc, isrLoc, fixStra)
    write_path = '.'.join(sourceCode.split('.')[0:-1]) + "FIXED."+sourceCode.split('.')[-1]
    writeCodes(newCodes,write_path)


if __name__ == "__main__":
    main()
