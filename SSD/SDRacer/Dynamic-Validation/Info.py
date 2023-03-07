from cli import *

LOG_FILE_NAME = "log"
IF_PRINT = True
CELL=conf.viper.cell
PROCESSOR=current_processor
CPU_PATH = CELL.current_processor
INTERRUPT_TIMES = 10

IOAPIC_PATH = conf.viper.mb.sb.ioapic
INTERRUPT_PATH = conf.viper.mb.sb.isa.iface.simple_interrupt
VM = conf.viper.cell_context
BUFF_LEN = 1
#not very support for 32bit system
SIM_SYSTEM_ARCHITECTURE = 64
#[max uncontroled fun run time, max fun run time],
#the formor is -1 means unlimit the fun run,0 means ctrl fun for the first run
#the latter is max function run time
MAX_FUN_RUN_TIME = [1,3]

'''
MAGIC_INSTRUCTION_ENABLE = True
PuT_NAME = ["ReadASIC","WriteASIC","Task"]
ISR_NAME = ["IntProcess"]
SV_ADDR = [0xffff88007b25fd80]
SV_NAME = ["TBR"]
CONTROL_MODE_ENABLE = [10]
SYMBOL_FILE_NAME = "dataRaceSymbol"
aName = "dataRace"

MAGIC_INSTRUCTION_ENABLE = True
PuT_NAME = ["irq_handler1"]
ISR_NAME = ["irq_handler2"]
SV_ADDR = [0xffffffffa00ec424]
SV_NAME = ["bufferRemainCap"]
CONTROL_MODE_ENABLE = [10,11]
SYMBOL_FILE_NAME = "exampleSymbol"
aName = "example"

MAGIC_INSTRUCTION_ENABLE = False
PuT_NAME = ["irq_handler1"]
ISR_NAME = ["irq_handler2"]
SV_ADDR = [0xffffffffa00ec464]
SV_NAME = ["bufferRemainCap"]
CONTROL_MODE_ENABLE = [10,11]
SYMBOL_FILE_NAME = "magicSymbol"
aName = "example"


MAGIC_INSTRUCTION_ENABLE = True
PuT_NAME = ["task2"]
ISR_NAME = ["irq_handler1"]
SV_ADDR = [0xffffffffa00ec460]
SV_NAME = ["packetsNumber"]
CONTROL_MODE_ENABLE = [10]
SYMBOL_FILE_NAME = "magic3Symbol"
aName = "example"


MAGIC_INSTRUCTION_ENABLE = True
PuT_NAME = ["task2"]
ISR_NAME = ["irq_handler1"]
SV_ADDR = [0xffffffffa00ec458]
SV_NAME = ["bufferAddr"]
CONTROL_MODE_ENABLE = [10]
SYMBOL_FILE_NAME = "magic2Symbol"
aName = "example"


#The following is for magic4
MAGIC_INSTRUCTION_ENABLE = True
PuT_NAME = ["task1"]
ISR_NAME = ["irq_handler1"]
#PuT_NAME = ["irq_handler1"]
SV_ADDR = [0xffffffffa00ec458]
SV_NAME = ["bufferAddr"]
CONTROL_MODE_ENABLE = [10]
SYMBOL_FILE_NAME = "magic4Symbol"
aName = "example"


#The following is for magic-short
MAGIC_INSTRUCTION_ENABLE = True
PuT_NAME = ["short_selfprobe"]
ISR_NAME = ["short_probing"]
#PuT_NAME = ["irq_handler1"]
SV_ADDR = [0xffffffffa00ed5e0]
SV_NAME = ["short_irq"]
CONTROL_MODE_ENABLE = [7]
SYMBOL_FILE_NAME = "shortSymbol"
aName = "short"


#The following is for magic-short inject error
MAGIC_INSTRUCTION_ENABLE = True
PuT_NAME = ["short_i_read"]
ISR_NAME = ["short_interrupt"]
#PuT_NAME = ["irq_handler1"]
SV_ADDR = [0xffffffffa00edad8]
SV_NAME = ["short_head"]
CONTROL_MODE_ENABLE = [7]
SYMBOL_FILE_NAME = "shortIESymbol"
aName = "short"

#MAGIC_INSTRUCTION is disabled
MAGIC_INSTRUCTION_ENABLE = False
PuT_NAME = ["serial8250_start_tx","transmit_chars"] 
ISR_NAME = ["serial8250_interrupt"]
SV_ADDR = [0xffff8100379c4654]
SV_NAME = ["xmit->tail"]
CONTROL_MODE_ENABLE = [4]
SYMBOL_FILE_NAME = "System.map-2.6.16"
aName = ""


#The following is for shortprint Un IE
MAGIC_INSTRUCTION_ENABLE = True
PuT_NAME = ["shortp_do_work"]
ISR_NAME = ["shortp_interrupt"]
SV_ADDR = [0xffffffffa00ed2a0]
SV_NAME = ["shortp_tv"]
CONTROL_MODE_ENABLE = [7]
SYMBOL_FILE_NAME = "shortprintUnIESymbol"
aName = "shortprint"


#The following is for shortprint Un IE
MAGIC_INSTRUCTION_ENABLE = True
PuT_NAME = ["shortp_read"]
ISR_NAME = ["shortp_interrupt"]
SV_ADDR = [0xffffffffa00ed288]
SV_NAME = ["shortp_in_head"]
CONTROL_MODE_ENABLE = [7]
SYMBOL_FILE_NAME = "shortprintIESymbol"
aName = "shortprint"


#The following is for lineBox
tmpPuT=["mainFun","timer0","Go","Go"]
tmpISR=["xint1","xint1","timer0","timer0"]
tmpSV_ADDR=[0xffffffffa00ec5e8,0xffffffffa00ec5d4,0xffffffffa00ec5d0,0xffffffffa00ec5a8]
tmpSV_NAME=["counter","DistanceSteps","i2","i1"]
tmpIN=[[10],[11,10],[11],[11]]
raceNum = 3
MAGIC_INSTRUCTION_ENABLE = False
SYMBOL_FILE_NAME = "lineboxSymbol"
aName = "linebox"
PuT_NAME = [tmpPuT[raceNum]]
ISR_NAME = [tmpISR[raceNum]]
SV_ADDR = [tmpSV_ADDR[raceNum]]
SV_NAME = [tmpSV_NAME[raceNum]]
CONTROL_MODE_ENABLE = tmpIN[raceNum]


#The following is for power
tmpPuT=["mainFun","mainFun"]
tmpISR=["RsManageA","RsManageB"]
tmpSV_ADDR=[0xffffffffa00ec26c,0xffffffffa00ec26c]
tmpSV_NAME=["downHeatNumber","downHeatNumber"]
tmpIN=[[10],[11]]
raceNum = 1
MAGIC_INSTRUCTION_ENABLE = False
SYMBOL_FILE_NAME = "powerSymbol"
aName = "power"
PuT_NAME = [tmpPuT[raceNum]]
ISR_NAME = [tmpISR[raceNum]]
SV_ADDR = [tmpSV_ADDR[raceNum]]
SV_NAME = [tmpSV_NAME[raceNum]]
CONTROL_MODE_ENABLE = tmpIN[raceNum]

#The following is for unoptimited power
tmpPuT=["DataSave","DataSave"]
tmpISR=["RsManageA","RsManageB"]
tmpSV_ADDR=[0xffffffffa00ec2cc,0xffffffffa00ec2cc]
tmpSV_NAME=["downHeatNumber","downHeatNumber"]
tmpIN=[[10],[11]]
raceNum = 0
MAGIC_INSTRUCTION_ENABLE = False
SYMBOL_FILE_NAME = "powerO0Symbol"
aName = "power"
PuT_NAME = [tmpPuT[raceNum]]
ISR_NAME = [tmpISR[raceNum]]
SV_ADDR = [tmpSV_ADDR[raceNum]]
SV_NAME = [tmpSV_NAME[raceNum]]
CONTROL_MODE_ENABLE = tmpIN[raceNum]
'''
#The following is for optimited power built in linux kernel
tmpPuT=["mainFun","mainFun"]
tmpISR=["RsManageA","RsManageB"]
#if tmpSV_ADDR is empty, we will search it in symbol table
tmpSV_ADDR=[None,None]
tmpSV_NAME=["downHeatNumber","downHeatNumber"]
tmpIN=[[10],[11]]
raceNum = 0
MAGIC_INSTRUCTION_ENABLE = False
SYMBOL_FILE_NAME = "System.map-2.6.16"
aName = ""
PuT_NAME = [tmpPuT[raceNum]]
ISR_NAME = [tmpISR[raceNum]]
SV_ADDR = [tmpSV_ADDR[raceNum]]
SV_NAME = [tmpSV_NAME[raceNum]]
CONTROL_MODE_ENABLE = tmpIN[raceNum]


#aName == "" means the name doesn't exist in symbol file
#CONTROL_MODE_ENABLE == None means it doesn't invoke interrupt
#CONTROL_MODE_ENABLE:The last irq is highest priority
