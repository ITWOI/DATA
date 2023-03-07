import csv
import os
import sys

def storeFile(csvfile, resFile, newData):
    csvfile.close()
    with open(resFile, 'wb') as writeFile:
        writer = csv.writer(writeFile)
        writer.writerows(newData)
        exit(0)

if __name__=='__main__':
    if len(sys.argv) != 4:
        error("Usage %s <fileName> <resFile> <outputDir> " % args[0])
    filePath = sys.argv[1]
    resFilePath = sys.argv[2]
    outputDir = sys.argv[3]
    fileName = os.path.basename(filePath)
    newNameTmp = fileName.split(".")[0]+"-sym"
    with open(filePath, 'r') as sourceFile:
        sourceLines = sourceFile.readlines()
    with open(resFilePath, 'r') as resFile:
        newFileLines = sourceLines
        count = 0
        resFileLines = resFile.readlines()
        state = 0
        resFileLen = len(resFileLines)
        for index in range(resFileLen):
            line = resFileLines[index]
            if  line == "\n":
                count += 1
            line = line.strip()
            # find the ins pos
            if state == 1:
                sourceLine = int(line.split(" ")[1])
                state = 0
            # find the ins fun
            if index + 1 != resFileLen and resFileLines[index+1] == "\n":
                newFileLines[sourceLine - 1] += line + "(0, NULL, NULL);\n"
                newName = newNameTmp + str(count) + "." + fileName.split(".")[1]
                with open(newName, 'w') as sourceFile:
                    sourceFile.writelines(sourceLines)
            if line.endswith("SVS") or line.endswith("SVP"):
                state = 1
