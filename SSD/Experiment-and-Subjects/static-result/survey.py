import csv, os

def parseResFile(resFile):
    with open(resFile) as f:
        lines = f.readlines()
        reports = []
        tmpRes = [0, 0]
        index = 0
        for line in lines:
            line = line.strip()
            if line == "":
                reports.append(tmpRes)
                tmpRes = [0, 0]
                index = 0
            if line.find(": SV") != -1:
                continue
            if line.startswith("a.c"):
                index = 1
            d = line.split(" ")
            if len(d) != 2:
                continue
            if tmpRes[index] != 0:
                continue
            tmpRes[index] = int(d[1])
    return reports

def ifMerge(codes, lastLine, curLine):
    s = lastLine-1
    e = curLine-1
    if s+1 == e:
        return True
    for i in range(s+1, e):
        code = codes[i].strip()
        if code != "" and not code[0] in ["/", "{", "}"] and not code.startswith("else"):
            return False
    return True

def filterRep(res, cFile, resFile):
    print(resFile)
    reports = parseResFile(resFile)
    newReports = []
    for i in range(1, len(res)):
        if res[i] == "1" or res[i] == "4" or not res[i].isdigit():
            newReports.append(reports[i-1])
    task = set()
    isr = set()
    print("Ture Postive number: "+str(len(newReports)))
    for i in newReports:
        task.add(i[0])
        isr.add(i[1])
    task = sorted(task)
    isr = sorted(isr)
    with open(cFile) as f:
        codes = f.readlines()
    overall = []
    for e in [task, isr]:
        lastLine = 0
        insPos = []
        for d in e:
            if not ifMerge(codes, lastLine, d):
                insPos.append(d)
            lastLine = d
        #print(e)
        overall.append(len(insPos)*2)
    print("Interrupts operations number: "+str(overall[0]))
    print("Lock operations number: "+str(sum(overall)))



def main(csvF, fileDir):
    with open(csvF) as f:
        res = csv.reader(f)
        for row in res:
            fileName = row[0]+".c"
            filePath = os.path.join(fileDir, fileName)
            resPath = row[0]+".res"
            filterRep(row, filePath, resPath)


main("mergedRes.csv", "../test-cases")
