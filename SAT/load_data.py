# Author : Yu
# It's used as a tool for loading CNF and transforming CNF
# expanding all inputs to the input with maximun width and length

#TODO: may be we can propose a method estimate the "best" width and length. It seems we can use machine learning to achieve this.

import sys, time
import os, operator
import numpy as np
import pickle as pkl
import tensorflow as tf
import copy, random, argparse, threading

import utilities

labelCount = [0,0,0]
width = 0
length = 0
sampleNum_p = 1
ifPadCNF = False
isExcludeMatrix = 0
guessNum = 0

resLock = threading.Lock()

class ProgressBar:
    def __init__(self, count = 0, total = 0, width = 50):
        self.count = count
        self.total = total
        self.width = width
    def move(self):
        self.count += 1
    def log(self, s):
        sys.stdout.write(' ' * (self.width + 9) + '\r')
        sys.stdout.flush()
        if s != "":
            print(s)
        progress = self.width * self.count / self.total
        sys.stdout.write('{0:3}/{1:3}: '.format(self.count, self.total))
        sys.stdout.write('#' * progress + '-' * (self.width - progress) + '\r')
        if progress == self.width:
            sys.stdout.write('\n')
        sys.stdout.flush()

def _int64Array_feature(array):
      return tf.train.Feature(int64_list=tf.train.Int64List(value=array))

def _int64_feature(value):
      return tf.train.Feature(int64_list=tf.train.Int64List(value=[value]))

def _bytes_feature(value):
      return tf.train.Feature(bytes_list=tf.train.BytesList(value=[value]))

def _dtype_feature(ndarray):
    """match appropriate tf.train.Feature class with dtype of ndarray. """
    assert isinstance(ndarray, np.ndarray)
    dtype_ = ndarray.dtype
    if dtype_ == np.float64 or dtype_ == np.float32:
        return lambda array: tf.train.Feature(float_list=tf.train.FloatList(value=array))
    elif dtype_ == np.int64:
        return lambda array: tf.train.Feature(int64_list=tf.train.Int64List(value=array))
    else:
        raise ValueError("The input should be numpy ndarray. Instaed got {}".format(ndarray.dtype))

def findNearestRowByInd(data, index):
    if len(data) <= index :
        return len(data)
    row = data[index][0]
    while True:
        if index == 0:
            return index
        if data[index][0] != row:
            return index
        index -= 1

def resizeData(data):
    beginIndex = data[0][0]
    for i in range(len(data)):
        data[i][0] -= beginIndex
    return data

def splitInput(data, value, sampleNum):
    newData = []
    newValue = []
    if sampleNum < 2:
        newData.append(data)
        newValue.append(value)
        return newData, newValue
    interval = int(len(value) / sampleNum + len(value) / (sampleNum + 1))
    for i in range(sampleNum):
        beginIndex = int(random.random()*len(value))
        #beginIndex = i*(len(value)/(sampleNum+1))
        beginIndex = findNearestRowByInd(data, beginIndex)
        endIndex = beginIndex + interval
        endIndex = findNearestRowByInd(data, endIndex)
        tmp = copy.deepcopy(data[beginIndex:endIndex])
        tmp = resizeData(tmp)
        newData.append(tmp)
        newValue.append(value[beginIndex:endIndex])
    return newData, newValue

def padCNFs(data, values, numOfVar, numOfClause, MAXWidth, MAXLength):
    times = int(MAXWidth / numOfVar)
    for index in range(len(data)):
        for i in range(1, times):
            data.append([data[index][0], data[index][1]+i*numOfVar])
            values.append(values[index])
    return numOfVar, numOfClause

def analyzeCNFsInfo(f_list):
    maxVar = 0
    maxCla = 0
    for filename in f_list:
        if os.path.splitext(filename)[-1] != '.cnf' or os.path.splitext(filename)[-1] != '.dimacs':
            continue
        with open(filename, 'r') as f:
            for line in f:
                splited = line.split()
                if len(splited) == 0:
                    continue
                if line[0] in ('p'):
                    numOfVar= int(splited[2])
                    numOfClause= int(splited[3])
                    if maxVar < numOfVar:
                        maxVar = numOfVar
                    if maxCla < numOfClause:
                        maxCla = numOfClause
    return maxVar, maxCla

def transfer_data_by_TFR(dirName, savefile):
    global labelCount
    global width, length
    f_list = []
    sampleNum = sampleNum_p
    for aDir in dirName:
        for aFile in os.listdir(aDir):
            f_list.append(aDir + "/" + aFile)
    writer = tf.python_io.TFRecordWriter(savefile)
    testWriter = tf.python_io.TFRecordWriter(savefile+".tests")
    bar = ProgressBar(total = len(f_list))


    MAXWidth, MAXLength = analyzeCNFsInfo(f_list)

    for index, i in enumerate(f_list):
        if os.path.splitext(i)[-1] == '.cnf' or os.path.splitext(i)[-1] == '.dimacs':
            (numOfVar, numOfClause, data, value, label) = loadData_TFR(i, MAXWidth)
            # skip timeout res because we don't need them.
            if label[0] == -1:
                bar.move()
                bar.log("")
                continue
            if numOfVar > width:
                width = numOfVar
            if numOfClause > length:
                length = numOfClause
            #numOfVar, numOfClause = padCNFs(data, value, numOfVar, numOfClause, MAXWidth, MAXLength)
            data, value = splitInput(data, value, sampleNum)
            '''
            leftWidth = 16 - len(data[0])
            leftLength = 96 - len(data)
            data = np.pad(data, ((0, leftLength), (0, leftWidth)), 'constant')
            '''
            # Create a feature
            for subIndex in range(len(data)):
                if index % 4 == 0:
                    #utilities.writeToTFReacor(testWriter, label, numOfVar, numOfClause, data, value, subIndex, index)
                    writeSubMatrix(i, testWriter, index)
                else:
                    #utilities.writeToTFReacor(writer, label, numOfVar, numOfClause, data, value, subIndex, index)
                    writeSubMatrix(i, writer, index)

        bar.move()
        bar.log("")
    print("unsat:sat:timeout=:%d:%d:%d" % (labelCount[0], labelCount[1], labelCount[2]))
    writer.close()
    testWriter.close()
    sys.stdout.flush()
    storeInfo(savefile, width, length)

def writeSubMatrix(CNFFile, writer, index):
    if isExcludeMatrix == 0:
        return
    CNFs, Values, res = readFromFile(CNFFile)
    for i in range(len(Values)):
        utilities.writeToTFReacor(writer, [res[i]], 0, 0, CNFs, Values, i, index)


def storeInfo(savefile, width, length):
    savefile = ('.').join(savefile.split('.')[:-1])
    f = open(savefile+".info", "wb")
    pkl.dump((width, length), f, -1)
    f.close()

def loadInfo(savefile):
    savefile = ('.').join(savefile.split('.')[:-1])
    with open(savefile+".info", 'r') as f:
        return pkl.load(f)

def transfer_data_by_pkl(dirName, savefile):
    global labelCount
    f_list = []
    for aDir in dirName:
        for aFile in os.listdir(aDir):
            f_list.append(aDir + "/" + aFile)

    datas = [[],[],[]]
    labels = [[],[],[]]
    count = 0
    bar = ProgressBar(total = len(f_list))
    global width, length
    for i in f_list:
        if os.path.splitext(i)[-1] == '.cnf':
            (numOfVar, numOfClause, data, label) = loadData(i)
            if numOfVar > width:
                width = numOfVar
            if numOfClause > length:
                length = numOfClause
            index = label[0]
            datas[index].append(data)
            labels[index].append(label[0])
            count += 1
        bar.move()
        bar.log("")
    print("")
    print("unsat:sat:timeout=:%d:%d:%d" % (labelCount[0], labelCount[1], labelCount[2]))
    datas, labels = normalizeInputs(datas, labels)
    datas = resizeInputs(width, length, datas)
    datas = np.array(datas)
    labels = np.array(labels)
    #checkInputs(width, length, datas)
    f = open(savefile, "wb")
    pkl.dump((datas, labels), f, -1)
    f.close()
    storeInfo(savefile, width, length)

def normalizeInputs(data, labels):
    targetSize = len(data[0])
    if len(data[1]) < targetSize:
        targetSize = len(data[1])
    data[0] = data[0][0:targetSize]
    data[1] = data[1][0:targetSize]
    labels[0] = labels[0][0:targetSize]
    labels[1] = labels[1][0:targetSize]
    print("Normalized unsat:sat:timeout=:%d:%d:%d" % (len(data[0]), len(data[1]), len(data[2])))
    data = data[0] + data[1] + data[2]
    labels = labels[0] + labels[1] + labels[2]
    return data, labels

def resizeInputs(width, length, samples):
    for index in range(len(samples)):
        data = samples[index]
        leftWidth = width - len(data[0])
        leftLength = length - len(data)
        samples[index] = np.pad(data, ((0, leftLength), (0, leftWidth)), 'constant')
    return np.array(samples)

def checkInputs(width, length, *samples):
    for data1 in samples:
        for data in data1:
            if len(data) != length or len(data[0]) !=  width:
                print("Check Error")

def loadData_TFR(filename, MAXWidth):
    data = []
    labels = []
    numOfVar = 0
    numOfClause = 0
    with open(filename, 'r') as f:
        for line in f:
            splited = line.split()
            if len(splited) == 0:
                continue
            if line[0] in ('p'):
                numOfVar= int(splited[2])
                numOfClause= int(splited[3])
            if line[0] not in ('c', 'p'):
                outData = splited[0:-1]
                data.append(outData)
    labels = loadLabel(filename)
    data, value = processCNF_TFR(data, numOfVar, MAXWidth)
    return (numOfVar, numOfClause, data, value, labels)


def processCNF_TFR(data, numOfVar, MAXWidth):
    outData = []
    value = []
    row = 0
    times = 1
    if ifPadCNF:
        times = int(MAXWidth/numOfVar)
    for line in data:
        tmpOut = []
        tmpLabels = []
        for var in line:
            varInt = int(var)
            label = 1.0
            if varInt > 0:
                varInt = varInt - 1
                label = 1.0
            elif varInt < 0:
                varInt = -varInt - 1
                label = -1.0
            '''
            # used to seperate res
            outData.append([row, varInt])
            #outData.append([row, int(MAXWidth/numOfVar)*varInt])
            value.append(label)
            '''
            # used to duplicated res
            tmpOut.append([row, varInt])
            tmpLabels.append(label)
        for i in range(times):
            out = copy.deepcopy(tmpOut)
            labels = copy.deepcopy(tmpLabels)
            for index in range(len(out)):
                out[index] = [out[index][0], out[index][1] + i*numOfVar]
            outData += out
            value += labels
        row += 1
    return outData, value

def loadData(filename):
    data = []
    labels = []
    numOfVar = 0
    numOfClause = 0
    with open(filename, 'r') as f:
        for line in f:
            splited = line.split()
            if len(splited) == 0:
                continue
            if line[0] in ('p'):
                numOfVar= int(splited[2])
                numOfClause= int(splited[3])
            if line[0] not in ('c', 'p'):
                outData = splited[0:-1]
                data.append(outData)
    labels = loadLabel(filename)
    data = processCNF(data, numOfVar)
    return (numOfVar, numOfClause, data, labels)

def processCNF(data, numOfVar):
    outData = []
    for line in data:
        vect = [0] * numOfVar
        for var in line:
            varInt = int(var)
            if varInt > 0:
                vect[varInt - 1] = 1
            elif varInt < 0:
                vect[-varInt - 1] = -1
        outData.append(vect)
    # disable the sort
    return outData
    # sort the array
    cols = []
    for i in range(0, numOfVar):
        cols.append(i)
    return sort_table(outData, cols)

def sort_table(table, cols):
    for col in reversed(cols):
        table = sorted(table, key=operator.itemgetter(col))
    return table

def loadLabel(filename):
    global labelCount
    label = []
    (shortname, extension) = os.path.splitext(filename)
    with open(shortname + ".res", 'r') as f:
        #the maximun lines are 3
        lines = f.read().splitlines()
        if lines[0] == "":
            label.append(-1)
            labelCount[-1] += 1
            label.append(int(lines[1]))
        elif lines[0] == "sat":
            label.append(1)
            labelCount[1] += 1
            label.append(int(lines[2]))
        elif lines[0] == "unsat":
            label.append(0)
            labelCount[0] += 1
            label.append(int(lines[1]))
        else:
            print("ERROR: exception in loadLabel.")
    return label

def loadPKL(dirName):
    f = open(dirName, "rb")
    data, labels= pkl.load(f)
    f.close()
    #print len(data), len(data[0]), len(data[0][0]), len(labels), len(labels[0])
    return data, labels

def main(argv):
    if (len(argv) > 1):
        savefile = argv[0]
        if savefile.endswith(".pkl"):
            transfer_data_by_pkl(argv[1:], savefile)
        elif savefile.endswith(".tfrecord"):
            transfer_data_by_TFR(argv[1:], savefile)
    else:
        loadPKL(dirName)
    print("width:%d, length:%d " % (width, length))

def writeToFile(CNF, Value, res, CNFFile):
    savefile = CNFFile+".extra"
    f = open(savefile, "wb")
    pkl.dump((CNF, Value, res), f, -1)
    f.close()

def readFromFile(savefile):
    fileName = savefile+".extra"
    if not os.path.exists(fileName):
        return [], [], []
    with open(savefile+".extra", 'r') as f:
        return pkl.load(f)

def generateCNF(CNFFile):
    resLock.acquire()
    (numOfVar, numOfClause, CNF, Value, label) = loadData_TFR(CNFFile, 0)
    resLock.release()
    res = [0 for i in range(numOfVar)]
    localGuessNum = guessNum if numOfVar > guessNum else numOfVar
    CNFs = []
    Values = []
    for subIndex in range(localGuessNum):
        index = int(random.random()*numOfVar)
        assign = random.choice([1,-1])
        # Changed it to modifiy one variable.
        newCNF, newValue, _ = utilities.GuessVar(CNF, Value, res, index, assign)
        if len(Value) != 0:
            CNFs.append(newCNF)
            Values.append(newValue)
        else:
            break
    res = []
    for subIndex in range(len(Values)):
        solvedRes = utilities.buildFormula(CNFs[subIndex], Values[subIndex])
        if solvedRes == "UNSAT":
            res.append(True)
        else:
            res.append(False)
    writeToFile(CNFs, Values, res, CNFFile)


def generateCNFs(dirName):
    global labelCount
    global width, length
    f_list = []
    sampleNum = sampleNum_p
    threadNum = 3
    for aDir in dirName:
        for aFile in os.listdir(aDir):
            f_list.append(aDir + "/" + aFile)
    bar = ProgressBar(total = len(f_list))

    threads = []

    for index, CNFFile in enumerate(f_list):
        if os.path.splitext(CNFFile)[-1] == '.cnf' and not os.path.exists(CNFFile+".extra"):
            generateCNF(CNFFile)
            '''
            t = threading.Thread(target=generateCNF, args=(CNFFile,))
            threads.append(t)
            t.start()
            if len(threads) == threadNum:
                for i in range(threadNum):
                    threads[i].join()
                threads = []
            '''


        bar.move()
        bar.log("")
    print("unsat:sat:timeout=:%d:%d:%d" % (labelCount[0], labelCount[1], labelCount[2]))
    sys.stdout.flush()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('files', metavar='N', type=str, nargs='+',
                                help='an integer for the accumulator')
    parser.add_argument('--sampleNum', type = int,default=1)
    parser.add_argument('--guessNum', type = int,default=0)
    parser.add_argument('--ESM', type = int,default=0)
    parser.add_argument('--padCNF', type = bool,default=False)
    args = parser.parse_args()
    sampleNum_p = args.sampleNum
    files = args.files
    ifPadCNF = args.padCNF
    guessNum = args.guessNum
    isExcludeMatrix = args.ESM
    if guessNum != 0 and ifPadCNF == False:
        generateCNFs(files[1:])
    elif guessNum != 0 and ifPadCNF:
        print("Conflict argument setting for guessNum and ifPadCNF.")
        exit()
    else:
        main(files)
