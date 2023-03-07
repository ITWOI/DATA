from __future__ import division, print_function, absolute_import

import tensorflow as tf
import sys, time, load_data, os, random
import numpy as np
from tensorflow.python.ops import array_ops
import datetime, copy
import pycosat, math

def _int64_feature(value):
    return tf.train.Feature(int64_list=tf.train.Int64List(value=[value]))

def _bytes_feature(value):
    return tf.train.Feature(bytes_list=tf.train.BytesList(value=[value]))

def parse_CNF(filename, length, width):
    filename_queue = tf.train.string_input_producer([filename])
    reader = tf.TFRecordReader()
    _, serialized_example = reader.read(filename_queue)
    features = tf.parse_single_example(
            serialized_example,
            # Defaults are not specified since both keys are required.
            features={
                'label': tf.FixedLenFeature([], tf.int64),
                'width': tf.FixedLenFeature([], tf.int64),
                'length': tf.FixedLenFeature([], tf.int64),
                'CNFWidth': tf.FixedLenFeature([], tf.int64),
                'CNFLength': tf.FixedLenFeature([], tf.int64),
                'CNF': tf.FixedLenFeature([], tf.string),
                'Value': tf.FixedLenFeature([], tf.string)
                })

    CNF = tf.decode_raw(features['CNF'], tf.int64)
    Value = tf.decode_raw(features['Value'], tf.float32)
    label = tf.cast(features['label'], tf.int64)
    CNFLength = tf.cast(features['CNFLength'], tf.int64)
    CNFWidth = tf.cast(features['CNFWidth'], tf.int64)
    inputLength = tf.cast(features['length'], tf.int64)
    inputWidth = tf.cast(features['width'], tf.int64)
    CNFshape = tf.stack([CNFLength, CNFWidth])
    CNF = tf.reshape(CNF, CNFshape)
    CNF = tf.sparse_to_dense(sparse_indices=CNF, sparse_values=Value, output_shape=[length, width])
    #CNF = tf.sparse_to_dense(sparse_indices=CNF, sparse_values=Value, output_shape=[inputLength, inputWidth])
    # 2 is the num of labels.
    label = tf.one_hot(label, 2)
    CNFshape = tf.stack([-1, length, width, 1])
    #CNFshape = tf.stack([-1, CNFLength, CNFWidth, 1])
    CNF = tf.reshape(CNF, shape=CNFshape)
    #CNF = tf.SparseTensor(indices=CNF, values=Value, dense_shape=[length,width])

    return CNF, label

def GuessVar(CNF, Value, res, index, varRes):
    newCNF = []
    newValue = []
    tmpCNF = []
    tmpValue = []
    isTrue = False
    res[index] = varRes
    i = 0
    lastRow = CNF[i][0]
    rowIndex = []
    # None: can not decide sat.
    # True or False: sat or unsat
    cnfRet = None
    ret = None
    targetLen = 0
    for cnfInd in range(len(CNF)):
        if CNF[cnfInd][0] != lastRow:
            lastRow = CNF[cnfInd][0]
            newCNF += tmpCNF
            newValue += tmpValue
            tmpCNF = []
            tmpValue = []
            isTrue = False

        if CNF[cnfInd][1] == index:
            if varRes * Value[cnfInd] == -1:
                cnfRet = False
                continue
            else:
                isTrue = True
                if cnfRet == None:
                    cnfRet = True
        else:
            tmpCNF.append(CNF[cnfInd])
            tmpValue.append(Value[cnfInd])
        if isTrue:
            if len(tmpCNF) != 0:
                tmpCNF = []
                tmpValue = []
            continue
    if cnfInd == len(CNF) - 1:
        newCNF += tmpCNF
        newValue += tmpValue
        targetLen = len(newCNF)

    '''
    while i < len(CNF):
        if CNF[i][0] != lastRow:
            rowIndex = []
            lastRow = CNF[i][0]
        rowIndex.append(i)

        if CNF[i][1] == index:
            if varRes * Value[i] == -1:
                CNF.pop(i)
                Value.pop(i)
                i -= 1
                rowIndex = []
                ret = False
            else:
                for popIndex in rowIndex[::-1]:
                    CNF.pop(popIndex)
                    Value.pop(popIndex)
                    i -= 1
                i += 1
                while i < len(CNF) and CNF[i][0] == lastRow:
                    CNF.pop(popIndex)
                    Value.pop(popIndex)
                if ret == None:
                    ret = True
                i -= 1
        i += 1
    targetLen = len(CNF)
    '''

    if len(newCNF) == 0:
        return newCNF, newValue, cnfRet
    else:
        return newCNF, newValue, None

def setForm(CNF, Value, assignments):
    res = [0 for i in range(len(assignments))]
    for i in range(len(assignments)):
        if assignments[i] == 0:
            continue
        val = 1
        if assignments[i] == False or assignments[i] == -1:
            val = -1
        CNF, Value, ret = GuessVar(CNF, Value, res, i, val)
        if len(CNF) == 0:
            return CNF, Value, ret
    return CNF, Value, ret

def checkForm(CNF, Value, assignments):
    res = [0 for i in range(len(assignments))]
    for i in range(len(assignments)):
        val = 1
        if assignments[i] == False or assignments[i] == -1:
            val = -1
        CNF, Value, ret = GuessVar(CNF, Value, res, i, val)
        if len(CNF) == 0:
            return ret
    return ret
'''
CNF=[[0,0],[0,1],[1,0],[1,1],[2,0],[2,1]]
Value=[1, 1, -1, 1, 1, -1]
assignments=[True,False]
checkForm(CNF, Value, assignments)
print("a")
'''

def buildFormula(CNF, Value):
    cnf = []
    clause = []
    lineNum = CNF[0][0]
    for i, data in enumerate(CNF):
        if lineNum != data[0]:
            cnf.append(clause)
            clause = []
            lineNum = data[0]
        val = int(Value[i] * (data[1] + 1))
        clause.append(val)

    cnf.append(clause)
    res = pycosat.solve(cnf)
    return res


def writeToTFReacor(writer, label, numOfVar, numOfClause, data, value, subIndex, index):
    feature = {'label': _int64_feature(label[0]),
            'width': _int64_feature(numOfVar),
            'length': _int64_feature(numOfClause),
            'index': _int64_feature(index),
            'CNFWidth': _int64_feature(len(data[subIndex][0])),
            'CNFLength': _int64_feature(len(data[subIndex])),
            'CNF': _bytes_feature(np.array(data[subIndex]).tobytes()),
            'Value': _bytes_feature(np.array(value[subIndex], dtype=np.float32).tobytes())}
    # Create an example protocol buffer
    example = tf.train.Example(features=tf.train.Features(feature=feature))
    # Serialize to string and write on the file
    writer.write(example.SerializeToString())
