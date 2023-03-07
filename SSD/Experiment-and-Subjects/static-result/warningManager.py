import csv
import os
import sys

def printInfo(fileName):
    print("Warnings are in file %s. Please choose the type of warning:" % fileName)
    print("0: Exit.")
    print("1: True race.")
    print("2: False negative, could be verified by simulation.")
    print("3: False negative, could be verified by symbolic execution.")
    print("4: True race, but need specific operation.")
    print("5: Others.")

def storeFile(csvfile, resFile, newData):
    csvfile.close()
    with open(resFile, 'wb') as writeFile:
        writer = csv.writer(writeFile)
        writer.writerows(newData)
        exit(0)

if __name__=='__main__':
    if len(sys.argv) != 4:
        error("Usage %s <inputDir> <sourceDir> <result.csv> " % args[0])
    outputDir = sys.argv[1]
    if not outputDir.endswith("/"):
        outputDir = outputDir + "/"
    sourceDir = sys.argv[2]
    if not sourceDir.endswith("/"):
        sourceDir += "/"
    resFileName = sys.argv[3]
    newData = []
    userInput = []
    with open(resFileName, 'rb') as csvfile:
        reader = csv.reader(csvfile)
        for row in reader:
            newData.append(row)
        for index in range(len(newData)):
            resFilePath = outputDir+newData[index][0] + ".res"
            sourceFilePath = sourceDir + newData[index][0] + ".c"
            count = 0
            resLen = len(newData[index]) - 1
            ifPrint = False
            with open(sourceFilePath, 'r') as sourceFile:
                sourceFileLines = sourceFile.readlines()
            with open(resFilePath, 'r') as resFile:
                resFileLines = resFile.readlines()
                for line in resFileLines:
                    if count < resLen:
                        if line == "\n":
                            count += 1
                        continue
                    line = line.strip()
                    if ifPrint:
                        ifPrint = False
                        sourceLine = line.split(" ")
                        print sourceFileLines[int(sourceLine[1])-1]
                        print line
                    if line == "":
                        printInfo(newData[index][0])
                        res = raw_input()
                        if res == "0":
                            print ",".join(userInput)
                            storeFile(csvfile, resFileName, newData)
                        userInput.append(res)
                        newData[index].append(res)
                    if line.endswith(":0") or line.endswith("SVS") or line.endswith("SVP"):
                        ifPrint = True
            print ",".join(userInput)
    storeFile(csvfile, resFileName, newData)
