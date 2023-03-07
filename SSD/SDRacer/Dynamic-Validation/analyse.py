
INIT = 0
PUT_SV_ACCESSED = 1
ISR_ENTRY = 2
ISR_SV_ACCESSED = 3
ISR_EXIT = 4
IRETD = 5
NEXT_PC = 6
RACE_DETECTED = -1

#When encount two or more "PuT:" state, it will get the lastest expect pc
def analyseLine(line,curState):
    if curState[0] == INIT:
        if "PuT:" not in line:
            return curState
        return [PUT_SV_ACCESSED,int(line.split(" ")[-1],16)]
    elif curState[0] == PUT_SV_ACCESSED:
        if "ISR_entry" in line:
            return [ISR_ENTRY,curState[1]]
        elif "PuT:" in line:
            return [PUT_SV_ACCESSED,int(line.split(" ")[-1],16)]
        else:
            print "analyse:state transform error",line
    elif curState[0] == ISR_ENTRY:
        if "ISR:"  in line:
            return [ISR_SV_ACCESSED,curState[1]]
        elif "ISR_exit" in line:
            print "Unlogged Info:No SV access in ISR"
            return [ISR_EXIT,curState[1]]
    elif curState[0] == ISR_SV_ACCESSED:
        if "ISR_exit" in line:
            return [ISR_EXIT,curState[1]]
        elif "ISR:" in line:
            return curState
        else:
            print "analyse:state transform error",line
            return [INIT,-1]
    elif curState[0] == ISR_EXIT:
        if "Iretd" in line:
            return [IRETD,curState[1]]
        else:
            return [INIT,-1]
    elif curState[0] == IRETD:
        nextPc = int(line,16)
        if nextPc != curState[1]:
            return [INIT,-1]
        return [RACE_DETECTED,-1]
    elif curState[0] == RACE_DETECTED:
        return [INIT,-1]
    else:
        print "ALERT: Unexpect type in log",line
        print curState[0]
        return [INIT,-1]


