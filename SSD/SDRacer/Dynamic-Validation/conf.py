import os
import platform
import copy
import datetime

import Info
import analyse

from cli import *

SYMBOL_FILE_NAME = Info.SYMBOL_FILE_NAME
LOG_FILE_NAME = Info.LOG_FILE_NAME
svAddrs = Info.SV_ADDR
procName = Info.aName
svName = Info.SV_NAME
controlEnable = Info.CONTROL_MODE_ENABLE
ifPrint = Info.IF_PRINT
cpuPath = Info.CPU_PATH
ioapicPath = Info.IOAPIC_PATH
interruptPath = Info.INTERRUPT_PATH
vm = Info.VM
stackLength = Info.SIM_SYSTEM_ARCHITECTURE / 8
buffLen = Info.BUFF_LEN
magicEnabel = Info.MAGIC_INSTRUCTION_ENABLE
isrName = Info.ISR_NAME
putName = Info.PuT_NAME
maxUnCtrlFunRunTimes = Info.MAX_FUN_RUN_TIME[0]
maxFunRunTime = Info.MAX_FUN_RUN_TIME[1]
#single processor

#function stack item for identify different function.
class func_stack_item(object):
    func_addr = 0x0;
    ebp = 0;
    esp = 0;
#stack
func_stack = []
is_ISR = False
ifInterruptInvoked = False
ebp_switch = -1
ISREntry = []
fEntry = []
logBuff = []
#this is used for detecting two interrupts. 
curState = [analyse.INIT,-1]
#this is used for storing magic instruction,the first element is interrupts
#the second is PuTs
prevMaigc = [[],[]]
prevStateOfMagic = [[],[]]
#only if put and interrupt both contain one fun
#isTimeToInterrupt is used by magic instruction,so it's true when magic not used
if magicEnabel:
    isTimeToInterrupt = 0
else:
    isTimeToInterrupt = Info.INTERRUPT_TIMES
#record fun run times
funRunTimes = [[],[]]
#store whether a fun is reverse executing
ifFunRevexec = [[],[]]
#record bookmark's name for other core_stop hap
bookmarkName = ""
#one element is {'cfg':CFG_Graphy,'ifenable':False,'cmpSkiped':0}
funDisCFG = [[],[]]
#store next addr for sir or put
nextFunAddr = [[],[]]
#store hap index and breakpoint id
indexList = [[],[]]
#used to control continue function
ifNeedConCallBack = False
#use current processor to gain all cpu information, the side effect is not known

def getCurrentCpu():
    return Info.CELL.current_processor

def ISR_enabled(irqnum):
    tmp1 = cpuPath.eflags & 0b1000000000
    ioapic = ioapicPath
    #in ioapic.redirection[irqnum], the first bit indicate wether whether 
    #the bit controlling the UART device is masked or not. 0x10000 means masked
    if tmp1 != 0 and (ioapic.redirection[irqnum] & 0x10000 == 0) and \
        ioapic.pin_raised[irqnum] == 0:
        return True
    return False

def enableInterrupt(irqNums):
    global ifInterruptInvoked,isTimeToInterrupt
    #conf.viper.mb.sb.pic.ports.irq[irqnum].signal.signal_raise()
    #conf.viper.mb.sb.pic.ports.irq[irqnum].signal.signal_lower()
    if ifInterruptInvoked:
        return
    #The last irq is highest priority
    irqnum = irqNums[-1]
    if isTimeToInterrupt and ISR_enabled(irqnum):
        #print "IRQ:%d enabled" % irqnum
        invokeInterrupt(irqnum)
        ifInterruptInvoked = True
        isTimeToInterrupt = isTimeToInterrupt - 1
        #SIM_run_alone(stopFun,irqnum)

def invokeInterrupt(irqnum):
    interruptPath.interrupt(irqnum)
    interruptPath.interrupt_clear(irqnum)
    logInfo("Interrupt:%s is invoked" % irqnum,False)

def expectPC(data):
    dis = SIM_disassemble_address(cpuPath,data[1],1,0)
    logInfo("PuT: $%s$, %s, %x" % (data[0],data[2],data[1]+dis[0]))

def rCallback(dummy, obj, bp, memop):
    global func_stack
    global is_ISR
    global ebp_switch
    current_ebp = cpuPath.rbp
    if func_stack:
        topItem = func_stack[-1]
        if current_ebp == topItem.ebp:
            if is_ISR == False:
                ebp_switch = current_ebp
                expectPC((svName[0],cpuPath.iface.processor_info.\
                                get_program_counter(),"Read"))
                if controlEnable != None:
                    enableInterrupt(controlEnable)
    
def wCallback(dummy, obj, bp, memop):
    current_ebp = cpuPath.rbp
    global func_stack
    global is_ISR
    global ebp_switch
    if func_stack:
        topItem = func_stack[-1]
        if current_ebp == topItem.ebp:
            if is_ISR == False:
                ebp_switch = current_ebp
                expectPC((svName[0],cpuPath.iface.processor_info.\
                                get_program_counter(),"Write"))
                if controlEnable != None:
                    enableInterrupt(controlEnable)
            else:
                logInfo("ISR: $%s$, write" % svName[0])
    
def isr_entry_callback(dummy, obj, bp, memop):
    global func_stack
    global is_ISR
    pushItem = func_stack_item()
    pushItem.func_addr = dummy
    pushItem.ebp = cpuPath.rsp - stackLength
    pushItem.esp = cpuPath.rsp
    func_stack.append(pushItem)
    logInfo("ISR_entry")
    is_ISR = True
    
def f_entry_callback(dummy, obj, bp, memop):
    global func_stack
    #push into stack
    pushItem = func_stack_item()
    pushItem.func_addr = int(dummy)
    #when this callback is called,the current instruction is 'push rbp' the
    #next instruction is mv rbp, rsp. so, ebp is current esp - 8
    pushItem.ebp = cpuPath.rsp - stackLength
    pushItem.esp = cpuPath.rsp
    func_stack.append(pushItem)
    #SIM_run_alone(simStop,(None))

def simStop(noUse):
    quiet_run_command("stop")
    
def ret_callback(dummy, obj, bp, memop):
    global func_stack
    global is_ISR
    current_esp = cpuPath.rsp
    if func_stack:
        topItem = func_stack[-1]
        if current_esp == topItem.esp:
            if topItem.func_addr in ISREntry:
                #if is_ISR == True:
                is_ISR = False
                logInfo("ISR_exit")
                #enableInterrupt(controlEnable)
            func_stack.pop()

def iretd_callback(dummy, obj, bp, memop):
    global func_stack
    current_ebp = cpuPath.rbp
    #print hex(current_ebp),hex(ebp_switch)
    if current_ebp == ebp_switch and True:
        logInfo("Iretd")
        addr = cpuPath.iface.processor_info.logical_to_physical(cpuPath.rsp,1)
        logInfo(str("%x" % SIM_read_phys_memory(cpuPath,addr.address,8)))
    
def readSymbol():
    lines = open(SYMBOL_FILE_NAME).readlines()
    symbolList = []
    for line in lines:
        splited = line.strip().split("\t")
        if len(splited) == 1:
            tmp = splited[0].split(" ")+[""]
        else:
            tmp = splited[0].split(" ")+[splited[1][1:][:-1]]
        symbolList.append(tmp)
    return symbolList

#get the "name" addr and the addr next to "name"
def getAddr(symbolList,name):
    if name == "":
        return -1
    symbolListLen = len(symbolList)
    for symbolIndex in range(symbolListLen):
        if symbolList[symbolIndex][2] == name and\
             symbolList[symbolIndex][3] == procName:
            addr = "0x"+symbolList[symbolIndex][0]
            if symbolIndex != symbolListLen:
                addrNext = "0x"+symbolList[symbolIndex + 1][0]
                count = 0
                newrestAddr = (1<<64)-1
                addr = int(addr,16)
                for i in range(symbolListLen):
                    tmpAddr = int("0x"+symbolList[i][0],16)
                    if((addr<tmpAddr) and tmpAddr<newrestAddr):
                        newrestAddr = tmpAddr
                return addr, newrestAddr
            else:
                return int(addr,16), 1 << 64
    logInfo("%s is not found in symbol." % (name),False)
    return -1,-1

def calWeight():
    weight = 0.0
    funCount = 0
    for ele in funDisCFG:
        for cfg in ele:
            if cfg.get('cmpCount') != None:
                weight += cfg['cmpSkiped']/float(cfg['cmpCount'])
            funCount += 1
    return 1 - weight / funCount

def logInfo(msg, ifAnalyseLine = True, ifAddTime = True,ifTerminal = False):
    global logBuff,curState,startTime
    if msg[-1] != '\n':
        msg += '\n'
    tmpMsg = getTime()+":"+msg
    if ifPrint and not ifAddTime:
        print msg,
    if ifPrint and ifAddTime:
        print tmpMsg,
    if ifAddTime:
        logBuff.append(tmpMsg)
    else:
        logBuff.append(msg)

    if len(logBuff) >= buffLen:
        fp = open(LOG_FILE_NAME,'a+')
        fp.writelines(logBuff)
        fp.close()
        logBuff = []
    if not ifAnalyseLine and ifTerminal == False:
        return
    curState = analyse.analyseLine(msg,curState)
    if curState[0] == analyse.RACE_DETECTED:
        if magicEnabel==False or ifAllPathSatified():
            logInfo("RACE DETECTED",False)
            logInfo("prio is:%.4f" %  calWeight(),False)
        curState = [analyse.INIT,-1]
        ifTerminal = True
    if ifTerminal:
        SIM_run_alone(quitSimics,None)

def quitSimics(noUse):
    run_command("quit")
    return

def ifAllPathSatified():
    global prevMaigc
    for magic in prevMaigc[0]:
        if magic != 3:
            return False
    for magic in prevMaigc[1]:
        if magic != 3:
            return False
    return True

def convertToInt(returnTuple):
    returnString = returnTuple[1]
    returnInt = returnString.split(' ')[1]
    return int(returnInt)

def controlRevexec(arg):
    if arg[1]:
        quiet_run_command("set-bookmark \"%s\"" % arg[0])
    else:
        quiet_run_command("delete-bookmark \"%s\"" % arg[0])

def skipToBookMark(data):
    global bookmarkName, funDisCFG,indexList,prevMaigc,ifNeedConCallBack
    prevMaigc[data[3]][data[4]] = 0
    bookmarkName = data[0]
    #add new breakpoint
    cfg = funDisCFG[data[3]]
    if cfg[data[4]].get('isEnable') != True:
        cfg[data[4]]['cfg'],cfg[data[4]]['cmpCount'] = disassembleToCFG(cpuPath\
                                                        ,data[1],data[2])
        instruction = cli.quiet_run_command('break %d %d' %\
                                         (data[1],data[2]-data[1]))
        instruction = convertToInt(instruction)
        cli.run_command('set-prefix %d "j"' %instruction)
        callbackIndex = SIM_hap_add_callback_index("Core_Breakpoint_Memop", \
                        revexec_callback, (data[3],data[4]),instruction)
        cfg[data[4]]['isEnable'] = True
        indexList[data[3]][data[4]] = [instruction,callbackIndex]
    quiet_run_command("stop")
    ifNeedConCallBack = True
    #VT_skipto_bookmark(name)
    #quiet_run_command("skip-to %s"%name)

def continueSim(noUse):
    global ifNeedConCallBack
    if ifNeedConCallBack == True:
        SIM_continue(0)
        ifNeedConCallBack = False
    return

def stop_callback(data,obj,exp,string):
    global bookmarkName,ifNeedConCallBack
    if ifNeedConCallBack == False:
        return
    if bookmarkName != "":
        logInfo("back to %s" % bookmarkName,False)
        VT_skipto_bookmark(bookmarkName)
        bookmarkName = ""
        SIM_run_alone(continueSim,None)
SIM_hap_add_callback("Core_Simulation_Stopped",stop_callback,None)

def revexec_callback(data,obj,exp,string):
    global funDisCFG
    cfg = funDisCFG[data[0]][data[1]]['cfg']
    pc = cpuPath.rip
    dis = SIM_disassemble_address(cpuPath,pc,1,0)
    if dis[1][0] != "j":
        return
    if dis[1][:3] == "jmp":
        return
    if cfg.get(pc) != None:
        funDisCFG[data[0]][data[1]]['cmpSkiped'] += 1
        cpuPath.rip = cfg.get(pc)

def deleteHapBreakpoint(arg):
    funType = arg[0]
    pos = arg[1]
    SIM_hap_delete_callback_id("Core_Breakpoint_Memop",\
                                indexList[funType][pos][1]);
    SIM_delete_breakpoint(indexList[funType][pos][0])

def maigc_callback(user_arg,cpu,arg):
    if magicEnabel == False:
        return
    global prevMaigc,func_stack,ifInterruptInvoked,isTimeToInterrupt,funRunTimes
    global ifFunRevexec,nextFunAddr,prevStateOfMagic
    bookMarkName = ""
    if func_stack[-1].func_addr in fEntry:
        funType = 1
        magicPos = prevMaigc[1]
        pos = fEntry.index(func_stack[-1].func_addr)
        funAddr = [func_stack[-1].func_addr,nextFunAddr[1][pos]]
        bookMarkName = str(putName[pos])
        #if is_ISR != False:
        #    print "In magicCallback: ISR's state is not the same"
    elif func_stack[-1].func_addr in ISREntry:
        funType = 0
        magicPos = prevMaigc[0]
        pos = ISREntry.index(func_stack[-1].func_addr)
        funAddr = [func_stack[-1].func_addr,nextFunAddr[0][pos]]
        bookMarkName = str(isrName[pos])
        #if is_ISR != True:
        #    print "In magicCallback: ISR's state is not the same"
    else:
        logInfo(func_stack[-1].func_addr,"not in list",False,ifTerminal = True)
        return
    if arg == 0:
        if maxUnCtrlFunRunTimes != -1 and funRunTimes[funType][pos]>\
           maxUnCtrlFunRunTimes-1:
            logInfo("begin reverse execution",False)
            if funRunTimes[funType][pos] == maxUnCtrlFunRunTimes:
                prevStateOfMagic = copy.deepcopy(prevMaigc)
            else:
                prevMaigc = copy.deepcopy(prevStateOfMagic)
                magicPos = prevMaigc[1]
            SIM_run_alone(controlRevexec,(bookMarkName,True))
            ifFunRevexec[funType][pos] = True
        if magicPos[pos] == -1 or magicPos[pos] == 3 or magicPos[pos] == 2:
            magicPos[pos] = 0
        else:
            logInfo("ERROR in MAGIC_CALLBACK, prevMaigc:%d,arg:%d" %(magicPos[pos],arg),False)
    elif arg == 1:
        if magicPos[pos] == 0:
            magicPos[pos] = 1
            if funType == 1:
                logInfo("%s,Encounted" % putName[pos],False)
                isTimeToInterrupt = 1
            else:
                logInfo("%s,Encounted" % isrName[pos],False)
            if ifFunRevexec[funType][pos]:
                SIM_run_alone(controlRevexec,(bookMarkName,False))
                ifFunRevexec[funType][pos] = False
                if len(indexList[funType][pos]) != 0:
                    SIM_run_alone(deleteHapBreakpoint,(funType,pos))
        elif magicPos[pos] == -1:
            if funType == 1:
                logInfo("Unreachable function: %s" % putName[pos],False,\
                        ifTerminal = True)
            else:
                logInfo("Unreachable function: %s" % isrName[pos],False,\
                        ifTerminal = True)
        elif magicPos[pos] == 3:
            #whichi maybe indicates it in a loop
            pass
        else:
            logInfo("ERROR in MAGIC_CALLBACK, prevMaigc:%d,arg:%d" %(magicPos[pos],arg),False)
        magicPos[pos] = 3
    elif arg == 2:
        if magicPos[pos] == 0:
            if funType == 1:
                logInfo("%s,Not encounted" % putName[pos],False)
            else:
                logInfo("%s,Not Encounted" % isrName[pos],False)
                ifInterruptInvoked = False
        elif magicPos[pos] != 3 and magicPos[pos] != 1:
            logInfo("ERROR in MAGIC_CALLBACK, prevMaigc:%d,arg:%d" %(magicPos[pos],arg),False)
        if ifFunRevexec[funType][pos]:
            SIM_run_alone(skipToBookMark,(bookMarkName,funAddr[0],funAddr[1],funType,pos))
        funRunTimes[funType][pos] += 1
        if funRunTimes[funType][pos] >= maxFunRunTime:
            logInfo("Run time out, may be SVs are not the same address",\
                    False,True,True)

def breakpointConfig():
    global fEntry,ISREntry,prevMaigc,putName,isrName,funRunTimes,indexList
    if os.path.isfile(LOG_FILE_NAME) == True:
        os.remove(LOG_FILE_NAME)
    logInfo("Current time:"+str(datetime.datetime.now()),False,False)
    if isrName[-1] == putName[-1]:
        logInfo("The same ISRs is a impossible race pair",False,True,True)
        return
    symbolList = readSymbol()
    for aIsrName in isrName:
        tmpISREntry,nextAddr = getAddr(symbolList,aIsrName)
        if tmpISREntry == -1:
            continue
        ISRBreakpoint = cli.quiet_run_command('break -x %d' % tmpISREntry)
        ISRBreakpoint = convertToInt(ISRBreakpoint)
        SIM_hap_add_callback_index("Core_Breakpoint_Memop", isr_entry_callback\
                                    ,tmpISREntry, ISRBreakpoint)
        retInstruction = cli.quiet_run_command('break %d %d' %\
                                           (tmpISREntry,nextAddr - tmpISREntry))
        retInstruction = convertToInt(retInstruction)
        cli.run_command('set-prefix %d "ret"' %retInstruction)
        SIM_hap_add_callback_index("Core_Breakpoint_Memop", ret_callback, \
                                    None, retInstruction)
        ISREntry.append(tmpISREntry)
        prevMaigc[0].append(-1)
        funRunTimes[0].append(0)
        ifFunRevexec[0].append(False)
        funDisCFG[0].append({'cfg':None,'isEnable':False,'cmpSkiped':0})
        nextFunAddr[0].append(nextAddr)
        indexList[0].append([])
        logInfo("%s is added to breakpoint." % (aIsrName),False)
    for aPuTName in putName:
        tmpfEntry,nextAddr = getAddr(symbolList,aPuTName)
        if tmpfEntry == -1:
            continue
        fBreakpoint = cli.quiet_run_command('break -x %d' % tmpfEntry)
        fBreakpoint = convertToInt(fBreakpoint)
        SIM_hap_add_callback_index("Core_Breakpoint_Memop", f_entry_callback, \
                                    tmpfEntry, fBreakpoint)
        retInstruction = cli.quiet_run_command('break %d %d' %\
                                               (tmpfEntry,nextAddr - tmpfEntry))
        retInstruction = convertToInt(retInstruction)
        cli.run_command('set-prefix %d "ret"' %retInstruction)
        SIM_hap_add_callback_index("Core_Breakpoint_Memop", ret_callback, \
                                    None, retInstruction)
        fEntry.append(tmpfEntry)
        prevMaigc[1].append(-1)
        funRunTimes[1].append(0)
        ifFunRevexec[1].append(False)
        funDisCFG[1].append({'cfg':None,'isEnable':False,'cmpSkiped':0})
        nextFunAddr[1].append(nextAddr)
        indexList[1].append([])
        logInfo("%s is added to breakpoint." % (aPuTName),False)
    for svIndex in range(len(svAddrs)):
        if svAddrs[svIndex] != None:
            svAddr = svAddrs[svIndex]
        else:
            svAddr = getAddr(symbolList,svName[svIndex])[0]
        if svAddr == -1:
            continue
        #svAddr += svOffset[svIndex]
        svReadBreakpoint = cli.quiet_run_command('break -r %d' % svAddr)
        svReadBreakpoint = convertToInt(svReadBreakpoint)
        SIM_hap_add_callback_index("Core_Breakpoint_Memop", rCallback, \
                                    svAddr, svReadBreakpoint)
        svWriteBreakpoint = cli.quiet_run_command('break -w %d' % svAddr)
        svWriteBreakpoint = convertToInt(svWriteBreakpoint)
        SIM_hap_add_callback_index("Core_Breakpoint_Memop", wCallback, \
                                    None, svWriteBreakpoint)
        logInfo("SV:%s is added to breakpoint." % (svName[svIndex]),False)

    iretdInstruction = cli.quiet_run_command('break 0 (1<<64)-1')
    iretdInstruction = convertToInt(iretdInstruction)
    cli.run_command('set-prefix %d "iret"' %iretdInstruction)
    SIM_hap_add_callback_index("Core_Breakpoint_Memop", iretd_callback,\
                                    None, iretdInstruction)
    SIM_hap_add_callback("Core_Magic_Instruction", maigc_callback, None)
    logInfo("Successfully install callback function!",False)
'''
def init():
    global stackLength
    cli.run_command("disassemble-settings opcode = on")
'''
#this fun returns cfg,cmpCount
def disassembleToCFG(cpuPath,addStart,addStop):
    if cpuPath.architecture == "x86-64":
        return analyseX86ToCFG(cpuPath,addStart,addStop)
    else:
        logInfo( "cpu architecture not supported",False)

def analyseX86ToCFG(cpuPath,addStart,addStop):
    cfg = {}
    addr = addStart
    instructs = []
    svIndex = 0
    count = 0
    cmpCount = 0
    while addr < addStop:
        dis = SIM_disassemble_address(cpuPath,addr,1,0)
        if instructs != []:
            instructs.append(list(dis))
            instructs[-1].append(addr)
        if "cpuid" in dis[1]:
            count += 1
            if instructs == []:
                instructs.append(list(dis))
                instructs[-1].append(addr)
            if count == 2:
                svIndex = len(instructs)
            if count == 3:
                break
        addr += dis[0]
    if addr > addStop:
        logInfo("In analyseX86ToCFG: wrong magic instruction",False)
    jmpAddr = 0
    for i in range(svIndex):
        instruct = instructs[svIndex - i]
        if instruct[1][:3] == "cmp":
            cmpCount += 1
        if len(instruct) == 1:
            continue
        if instruct[1][0].lower() != "j":
            continue
        dis = instruct[1].split(" ")
        if len(dis) != 2:
            logInfo("In analyseX86ToCFG: jump instruction is more than 2",False)
            continue
        if int(dis[1],16)>instructs[svIndex][2]:
            if instruct[1][:3] == "jmp":
                if jmpAddr != 0:
                    logInfo("Multi jmp encount.",False)
                jmpAddr = int(dis[1],16)
            else:
                #next instruction
                cfg[instruct[2]] = instruct[2] + instruct[0]
        elif jmpAddr != 0:
            if jmpAddr < int(dis[1],16):
                cfg[jmpAddr] = int(dis[1],16)
                jmpAddr = 0
    if jmpAddr != 0:
        logInfo("SV unreachable",False,ifTerminal = True)
    return cfg,cmpCount
        
def getTime():
    return str(datetime.datetime.now())

if __name__ == '__main__':
    #init()
    breakpointConfig()
    if len(controlEnable) != 1:
        if ISR_enabled(controlEnable[0]) == True:
            invokeInterrupt(controlEnable[0])
        else:
            logInfo("Can't invoke interrupt:%s" % controlEnable[0],False)
