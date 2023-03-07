from __future__ import division, print_function, absolute_import

import tensorflow as tf
import sys, time, load_data, os, random
import numpy as np
from scipy.sparse import coo_matrix
from tensorflow.python.ops import array_ops
import datetime, argparse, copy, math
import pickle as pkl
import utilities
import tfplot
import matplotlib.pyplot as plt
from math import sqrt
from PIL import Image




# Training Parameters
learning_rate = 0.001

# steps_cal = (no of ex / batch_size) * no_of_epochs
# if num_steps is less than steps_call, it only trains num_steps.
#num_steps = 2000
num_steps = None

batch_size = 4
width = 0
length = 0
firstLayerKernel = ()

sampleNum = 0
isVisualize = False

export_dir = "save_models"
basicInfoList = []
pck=[0,0,0,0]
flatPredClass=[0,0,0,0, 0, 0]
flatPredRes = [0, 0]
callCounter = 0

# Network Parameters
num_classes = 2 # MNIST total classes (0-9 digits)
dropout = 0.75 # Dropout, probability to keep units

tf.logging.set_verbosity(tf.logging.ERROR)

testLabels = []

def put_kernels_on_grid (kernel, pad = 1):
    # get shape of the grid. NumKernels == grid_Y * grid_X
    def factorization(n):
        for i in range(int(sqrt(float(n))), 0, -1):
            if n % i == 0:
                if i == 1: print('Who would enter a prime number of filters')
                return (i, int(n / i))
    (grid_Y, grid_X) = factorization (kernel.get_shape()[3].value)
    print ('grid: %d = (%d, %d)' % (kernel.get_shape()[3].value, grid_Y, grid_X))
    print(kernel.shape)
    x_min = tf.reduce_min(kernel)
    x_max = tf.reduce_max(kernel)
    kernel = (kernel - x_min) / (x_max - x_min)

    x = tf.pad(kernel, tf.constant( [[pad, pad],[pad, pad],[0,0],[0,0]] ), mode = 'CONSTANT')
    # X and Y dimensions, w.r.t. padding
    Y = kernel.get_shape()[0] + 2 * pad
    X = kernel.get_shape()[1] + 2 * pad
    channels = kernel.get_shape()[2]
    # put NumKernels to the 1st dimension
    x = tf.transpose(x, (3, 0, 1, 2))
    # organize grid on Y axis
    x = tf.reshape(x, tf.stack([grid_X, -1, X, channels]))
    # switch X and Y axes
    x = tf.transpose(x, (0, 2, 1, 3))
    # organize grid on X axis
    x = tf.reshape(x, tf.stack([1, X * grid_X, -1, channels]))
    print(x.shape)
    # back to normal order (not combining with the next step for clarity)
    x = tf.transpose(x, (2, 1, 3, 0))
    print(x.shape)
    # to tf.image_summary order [batch_size, height, width, channels],
    x = tf.transpose(x, (3, 0, 1, 2))
    print(x.shape)
    return x

def addSummary(weights, name):
    weightShape = weights.shape
    #weights_transposed = put_kernels_on_grid(weights)

    weights = tf.reduce_mean(weights, 0)
    x_min = tf.reduce_min(weights)
    x_max = tf.reduce_max(weights)
    weights_0_to_1 = (weights - x_min) / (x_max - x_min)
    weights_0_to_255_uint8 = tf.image.convert_image_dtype (weights_0_to_1, dtype=tf.uint8)

    weights_transposed = tf.reshape(weights, [1, weights.shape[0], -1 ,4])
    tf.summary.image(name, weights_transposed)
    return
    W_b = tf.split(weights, weightShape[3], 3)         # i.e. [32, 5, 5, 1, 1]
    rows = []
    for i in range(int(int(weightShape[3])/8)):
        x1 = i*8
        x2 = (i+1)*8
        row = tf.concat(W_b[x1:x2],0)
        rows.append(row)
    W_c = tf.concat(rows, 1)
    c_shape = W_c.get_shape().as_list()
    CNFshape = tf.stack([c_shape[2], -1, c_shape[1], 1])
    W_d = tf.reshape(W_c, CNFshape)
    print(W_d.shape)
    tf.summary.image(name, W_d)

# Create the neural network
def conv_net(x_dict, n_classes, dropout, reuse, is_training):
    # Define a scope for reusing the variables
    with tf.variable_scope('ConvNet', reuse=reuse):
        # TF Estimator input is a dict, in case of multiple inputs
        x = x_dict['CNF']

        #x = tf.reshape(x, shape=[-1, length, width, 1])

        # The data type should be float

        # Convolution Layer with 32 filters and a kernel size of 5
        conv0 = tf.layers.conv2d(x, 16, firstLayerKernel, firstLayerKernel, activation=tf.nn.relu)
        if isVisualize:
            addSummary(conv0, "conv0")
        # Max Pooling (down-sampling) with strides of 2 and kernel size of 2
        conv0 = tf.layers.max_pooling2d(conv0, 2, 2)

        # Convolution Layer with 32 filters and a kernel size of 5
        conv1 = tf.layers.conv2d(conv0, 32, 5, activation=tf.nn.relu)
        if isVisualize:
            addSummary(conv1, "conv1")
        # Max Pooling (down-sampling) with strides of 2 and kernel size of 2
        conv1 = tf.layers.max_pooling2d(conv1, 2, 2)

        # Convolution Layer with 64 filters and a kernel size of 3
        conv2 = tf.layers.conv2d(conv1, 64, 3, activation=tf.nn.relu)
        if isVisualize:
            addSummary(conv2, "conv2")
        # Max Pooling (down-sampling) with strides of 2 and kernel size of 2
        conv2 = tf.layers.max_pooling2d(conv2, 2, 2)

        # Flatten the data to a 1-D vector for the fully connected layer
        fc1 = tf.contrib.layers.flatten(conv2)

        # Fully connected layer (in tf contrib folder for now)
        fc1 = tf.layers.dense(fc1, 1024)
        # Apply Dropout (if is_training is False, dropout is not applied)
        fc1 = tf.layers.dropout(fc1, rate=dropout, training=is_training)

        # Output layer, class prediction
        out = tf.layers.dense(fc1, n_classes)

    return out


# Define the model function (following TF Estimator Template)
def model_fn(features, labels, mode):
    # Build the neural network
    # Because Dropout have different behavior at training and prediction time, we
    # need to create 2 distinct computation graphs that still share the same weights.
    logits_train = conv_net(features, num_classes, dropout, reuse=False,
                            is_training=True)
    logits_test = conv_net(features, num_classes, dropout, reuse=True,
                           is_training=False)

    # Predictions
    pred_classes = tf.argmax(logits_test, axis=1)
    pred_probas = tf.nn.softmax(logits_test)

    if isVisualize:
        summary_hook = tf.train.SummarySaverHook(
                100,
                output_dir='/tmp/tf',
                summary_op=tf.summary.merge_all())


    # If prediction mode, early return
    if mode == tf.estimator.ModeKeys.PREDICT:
        return tf.estimator.EstimatorSpec(mode, predictions=pred_classes,
                export_outputs={"classes": tf.estimator.export.PredictOutput({"labels": pred_classes})})

    # Define loss and optimizer
    loss_op = tf.reduce_mean(tf.nn.sparse_softmax_cross_entropy_with_logits(
        logits=logits_train, labels=tf.cast(labels, dtype=tf.int32)))
    optimizer = tf.train.AdamOptimizer(learning_rate=learning_rate)
    train_op = optimizer.minimize(loss_op,
                                  global_step=tf.train.get_global_step())

    # Evaluate the accuracy of the model
    acc_op = tf.metrics.accuracy(labels=labels, predictions=pred_classes)

    # TF Estimators requires to return a EstimatorSpec, that specify
    # the different ops for training, evaluating, ...
    if isVisualize:
        estim_specs = tf.estimator.EstimatorSpec(
                mode=mode,
                predictions=pred_classes,
                loss=loss_op,
                train_op=train_op,
                eval_metric_ops={'accuracy': acc_op},
                training_hooks=[summary_hook])
    else:
        estim_specs = tf.estimator.EstimatorSpec(
                mode=mode,
                predictions=pred_classes,
                loss=loss_op,
                train_op=train_op,
                eval_metric_ops={'accuracy': acc_op})


    return estim_specs

def evalPred(pred, labels, sec):
    count = 0
    right = 0.0
    totalNum = 0
    bias = [0,0,0,0]
    satOnsat = 0
    unsatOnUnsat = 0
    for i in pred:
        isCorrect = False
        if i == labels[count]:
            right += 1
            isCorrect = True
        if i == 0:
            bias[0] += 1
        else:
            bias[1] += 1
        if labels[count] == 0:
            bias[2] += 1
            if isCorrect:
                unsatOnUnsat += 1
        else:
            bias[3] += 1
            if isCorrect:
                satOnsat += 1
        totalNum += 1
        count += 1

    if bias[2] == 0:
        bias[2] == -1
    if bias[3] == 0:
        bias[3] == -1
    print("right: %d, totalNum: %d, Testing Accuracy:%f, Acc for SAT: %f, acc for UNSAT: %f, in %f seconds"%
            (right, totalNum,  right/totalNum, unsatOnUnsat/bias[2], satOnsat/bias[3], sec))
    print(bias)

def evalRes(model, input_fn, testLabels):
    begin = datatime.datatime.now()
    pred =  model.predict(input_fn)
    end = datetime.datetime.now()
    k = end - begin
    evalPred(pred, testLabels, k.total_seconds())

feature_spec = {
        'label': tf.FixedLenFeature([], tf.int64),
        'width': tf.FixedLenFeature([], tf.int64),
        'length': tf.FixedLenFeature([], tf.int64),
        'CNFWidth': tf.FixedLenFeature([], tf.int64),
        'CNFLength': tf.FixedLenFeature([], tf.int64),
        'CNF': tf.FixedLenFeature([], tf.string),
        'Value': tf.FixedLenFeature([], tf.string)
        }

def serving_input_receiver_fn():
    """An input receiver that expects a serialized tf.Example."""
    serialized_tf_example = tf.placeholder(dtype=tf.float32,
            shape=[None, length, width, 1],
            name='input_example_tensor')
    receiver_tensors = {'CNF': serialized_tf_example}
    #features = tf.parse_example(serialized_tf_example, feature_spec)
    return tf.estimator.export.ServingInputReceiver(receiver_tensors, receiver_tensors)

def my_input_fn(file_path, length, width, batch_size, perform_shuffle=True, repeat_count=1):
    def parse_CNF(tfrecord):
        features = tf.parse_single_example(
                tfrecord,
                # Defaults are not specified since both keys are required.
                features=feature_spec)


        CNF = tf.decode_raw(features['CNF'], tf.int64)
        Value = tf.decode_raw(features['Value'], tf.float32)
        label = tf.cast(features['label'], tf.int64)
        CNFLength = tf.cast(features['CNFLength'], tf.int64)
        CNFWidth = tf.cast(features['CNFWidth'], tf.int64)
        inputLength = tf.cast(features['length'], tf.int64)
        inputWidth = tf.cast(features['width'], tf.int64)
        CNFshape = tf.stack([CNFLength, CNFWidth])
        CNF = tf.reshape(CNF, CNFshape)
        # for neurosat's data, the clause is not sorted, so we shoud add validate_indices = False
        CNF = tf.sparse_to_dense(sparse_indices=CNF, sparse_values=Value, output_shape=[length, width], validate_indices = False)
        #CNF = tf.SparseTensor(indices=CNF, values=Value, dense_shape=[length,width])
        CNFshape = tf.stack([length, width, 1])
        CNF = tf.reshape(CNF, CNFshape)
        #CNF = tf.image.resize_image_with_crop_or_pad(CNF, 2000, 2000)

        '''
        # the TFRecord for the following codes is that "label, length, width, CNF". CNF is the transformed vectors.
        # for more information, refer to the previous commit.
        height = tf.shape(CNF)[0]
        imgWidth = tf.shape(CNF)[1]
        paddings = [[0, length - height],[0, width - imgWidth]]

        #CNF = array_ops.pad(CNF, paddings)
        CNF = tf.pad(CNF, paddings, "CONSTANT")
        '''

        return {"CNF":CNF}, label

    dataset = tf.data.TFRecordDataset(file_path)
    dataset = dataset.map(parse_CNF)
    if perform_shuffle:
        dataset = dataset.shuffle(buffer_size=256)
    dataset = dataset.repeat(repeat_count)  # Repeats dataset this # time
    dataset = dataset.batch(batch_size)  # Batch size to use
    iterator = dataset.make_one_shot_iterator()
    batch_features, batch_labels = iterator.get_next()
    return  batch_features, batch_labels

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

def run_pkl(args):
    # Build the Estimator
    global length, width
    model = tf.estimator.Estimator(model_fn)

    data, labels = load_data.loadPKL(args[0])
    data = data.tolist()
    #data = resizeInputs(width, length, data)
    #checkInputs(width, length, data)
    splitedBegin = int(len(data)/2 - len(data)/8)
    splitedEnd = int(len(data)/2 + len(data)/8)
    testData = data[splitedBegin:splitedEnd]
    testLabels = labels[splitedBegin:splitedEnd]
    data = np.concatenate((data[:splitedBegin - 1], data[splitedEnd + 1:]), axis=0)
    labels = np.concatenate((labels[:splitedBegin - 1], labels[splitedEnd + 1:]), axis=0)
    # Define the input function for training
    input_fn = tf.estimator.inputs.numpy_input_fn(
            x={'CNF': data}, y=labels,
            batch_size=batch_size, num_epochs=100, shuffle=True)

    # Train the Model
    model.train(input_fn, steps=num_steps)

    # Evaluate the Model
    # Use the Estimator 'evaluate' method
    '''
    e = model.evaluate(input_fn)
    print("Testing Accuracy:", e['accuracy'])
    '''

    # Define the input function for evaluating
    input_fn = tf.estimator.inputs.numpy_input_fn(
            x={'CNF': testData}, y=testLabels,
            batch_size=batch_size, shuffle=False)
    evalRes(model, input_fn, testLabels)

def execute_plot_op(image):
    print (">>> " + str(image))
    sess = tf.Session()
    with sess.as_default():   # or `with sess:` to close on exit
        ret = image.eval()
    plt.close()

    if len(ret.shape) == 3:
        # single image
        return Image.fromarray(ret)
    elif len(ret.shape) == 4:
        return [Image.fromarray(r) for r in ret]
    else:
        raise ValueError("Invalid rank : %d" % len(ret.shape))

def run_tfr(args):
    global length, width, batch_size
    config = tf.ConfigProto()
    config.gpu_options.allow_growth = True
    modConf = tf.estimator.RunConfig(session_config = config)

    model = tf.estimator.Estimator(model_fn)

    model.train(input_fn = lambda: my_input_fn(args[0], length, width, batch_size), steps=num_steps)

    # Evaluate the Mode
    # Use the Estimator 'evaluate' method

    if isSaveModel:
        model_path = model.export_savedmodel(export_dir, serving_input_receiver_fn)
        print(model_path)

        # Define the input function for training
        predictions = []
        predict_fn = tf.contrib.predictor.from_saved_model(model_path)
        labels=[]
        inputArray = []
        begin = datetime.datetime.now()
        for serialized_example in tf.python_io.tf_record_iterator(args[0]+".tests"):
            example = tf.train.Example()
            example.ParseFromString(serialized_example)
            label = example.features.feature['label'].int64_list.value[0]
            labels.append(label)
            inputArray.append(parseOneRecord(example))
        predictions = predict_fn({"CNF":np.array(inputArray)})["labels"]
        end = datetime.datetime.now()
        k = end - begin
        evalPred(predictions, labels, k.total_seconds())
        #model = tf.estimator.Estimator(model_fn, model_dir=model_path)
        exit()

    begin = datetime.datetime.now()
    e = model.evaluate(input_fn = lambda: my_input_fn(args[0]+".tests", length, width, batch_size))
    end = datetime.datetime.now()
    k = end - begin
    print("Testing Accuracy:%f, in %f seconds" % (e['accuracy'], k.total_seconds()))

    labels=[]
    begin = datetime.datetime.now()
    inputArray = []
    for serialized_example in tf.python_io.tf_record_iterator(args[0]+".tests"):
        example = tf.train.Example()
        example.ParseFromString(serialized_example)
        label = example.features.feature['label'].int64_list.value[0]
        labels.append(label)
        inputArray.append(parseOneRecord(example))
    numpy_input_fn = tf.estimator.inputs.numpy_input_fn(x={"CNF":np.array(inputArray)}, shuffle = False)
    mod1Labels = model.predict(input_fn = numpy_input_fn)
    predRes = (list(mod1Labels))

    end = datetime.datetime.now()
    k = end - begin
    evalPred(predRes, labels, k.total_seconds())
    print("in %f seconds"%
            (k.total_seconds()))

    if isVisualize:
        print("Visualizing")
        print(model.get_variable_names())
        weights = model.get_variable_value("ConvNet/conv2d_2/kernel")
        #plt.imshow(weights)
        #plt.show()
        def figure_heatmap(heatmap, cmap='jet'):
            # draw a heatmap with a colorbar
            fig, ax = tfplot.subplots(figsize=(4, 3))
            im = ax.imshow(heatmap, cmap=cmap)
            fig.colorbar(im)
            return fig


        '''
        plot_op = tfplot.plot(figure_heatmap, [weights], cmap='jet')
        image = execute_plot_op(plot_op)
        image.show()

        # Or just directly add an image summary with the plot
        tfplot.summary.plot("heatmap_summary", figure_heatmap, [weights])
        '''
    # Define the input function for evaluating
    #evalRes(model, lambda: my_input_fn(args[0]), testLabels)

def extractSparseMatrix(example):
    CNF = np.frombuffer(example.features.feature['CNF'].bytes_list.value[0], np.int64)
    Value = np.frombuffer(example.features.feature['Value'].bytes_list.value[0], np.float32)
    CNFLength = example.features.feature['CNFLength'].int64_list.value[0]
    CNFWidth = example.features.feature['CNFWidth'].int64_list.value[0]
    inputLength = example.features.feature['length'].int64_list.value[0]
    inputWidth = example.features.feature['width'].int64_list.value[0]
    '''
    newValue = []
    for i in range(len(Value)):
        newValue.append(-Value[i])
    CNF = np.reshape(CNF, [CNFLength, CNFWidth])
    return np.array(newValue), CNF, (inputLength, inputWidth)
    '''
    CNF = np.reshape(CNF, [CNFLength, CNFWidth])
    return Value, CNF, (inputLength, inputWidth)

def constructMatrix(Value, CNF):
    CNF = np.array(CNF)
    CNF = coo_matrix((Value,(CNF[:,0],CNF[:,1])), dtype=np.float32, shape=(length, width)).toarray()
    CNF = np.reshape(CNF, [length, width, 1])
    return CNF


def parseOneRecord(example):
    Value, CNF, _ = extractSparseMatrix(example)
    return constructMatrix(Value, CNF)

def concatRes(predRes, CNFRes):
    for i in range(len(predRes)):
        if predRes[i] == 0:
            predRes[i] = 1 if CNFRes[i]>0 else -1
    return predRes

def predAssignByGuess(mod1Labels, res, CNF, Value):
    for i in range(int(len(mod1Labels)/2)):
        lab1 = mod1Labels[2*i]
        lab2 = mod1Labels[2*i+1]
        if lab1 == 1 and lab2 == 1:
            res[i] = random.choice([-1, 1])
            flatPredClass[0] += 1
        elif lab1 == -1 and lab2 == -1:
            flatPredClass[1] += 1
            return False
        elif lab1 == 1:
            flatPredClass[2] += 1
            res[i] = 1
        else:
            flatPredClass[3] += 1
            res[i] = -1

    if utilities.checkForm(CNF, Value, res):
        flatPredRes[0] += 1
        return True
    return None

def predAssignByVer(mod1Labels, res, CNF, Value):
    ifVali = False
    for i in range(int(len(mod1Labels)/2)):
        lab1 = mod1Labels[2*i]
        lab2 = mod1Labels[2*i+1]
        if lab1 == 1 and lab2 == 1:
            res[i] = random.choice([-1, 1])
            flatPredClass[0] += 1
        else:
            flatPredClass[1] += 1
            ifVali = True
            res[i] = 0
    if ifVali:
        flatPredClass[2] += 1
    flatPredClass[3] += 1
    newCNF, newValue, ret = utilities.setForm(CNF, Value, res)
    if len(newValue) == 0:
        if ret == True:
            return True
        else:
            return None
    if utilities.buildFormula(newCNF, newValue):
        return True
    return None

def predictCNFValuesFlat(CNF, Value, inputShape, predict_fn):
    global pck, callCounter
    Value = Value.tolist()
    CNF = CNF.tolist()
    CNFs = []
    Values = []
    BackRes = []
    index = 0
    res = [0 for i in range(inputShape[1])]
    originalCNF = copy.deepcopy(CNF)
    originalValue = copy.deepcopy(Value)
    inputArray = []
    begin = datetime.datetime.now()
    mod1Labels = []

    for i in range(int(inputShape[1])):
        CNF, Value, _ = utilities.GuessVar(originalCNF, originalValue, res, i, 1)
        if len(Value) == 0:
            return [1]
        inputArray.append(constructMatrix(Value, CNF))
        CNF, Value, _ = utilities.GuessVar(originalCNF, originalValue, res, i, -1)
        if len(Value) == 0:
            return [1]
        inputArray.append(constructMatrix(Value, CNF))
        predict_fn({"CNF":np.array(inputArray)})["labels"]
        mod1Labels += list(predict_fn({"CNF":np.array(inputArray)})["labels"])
        inputArray = []
    end = datetime.datetime.now()
    k = end - begin
    pck[0] += k.total_seconds()

    '''
    numpy_input_fn = tf.estimator.inputs.numpy_input_fn(x={"CNF":np.array(inputArray)}, shuffle = False)
    mod1Labels = model.predict(input_fn = numpy_input_fn)
    mod1Labels = list(mod1Labels)
    '''
    begin1 = datetime.datetime.now()
    callCounter += 1
    for testTime in range(10):
        begin = datetime.datetime.now()
        ret = predAssignByGuess(mod1Labels, res, CNF, Value)
        #ret = predAssignByVer(mod1Labels, res, CNF, Value)
        if ret == True:
            end = datetime.datetime.now()
            k = end - begin
            pck[3] += k.total_seconds()
            break
        elif ret == False:
            return []
        end = datetime.datetime.now()
        k = end - begin
        pck[3] += k.total_seconds()
        flatPredClass[4] += 1
    flatPredClass[5] += 1
    end1 = datetime.datetime.now()
    k = end1 - begin1
    pck[1] += k.total_seconds()



    flatPredRes[1] += 1
    return res

def predictCNFValues(CNF, Value, inputShape, model):
    Value = Value.tolist()
    CNF = CNF.tolist()
    CNFs = []
    Values = []
    BackRes = []
    index = 0
    if solveMethod == 1:
        VerifiedRes = utilities.buildFormula(CNF, Value)
        if VerifiedRes != "UNSAT":
            return VerifiedRes
        else:
            return []

    res = [0 for i in range(inputShape[1])]
    while True:
        inputArray = []
        CNFT, ValueT, _ = utilities.GuessVar(CNF, Value, res, index, 1)
        if len(ValueT) == 0:
            break
        CNFF, ValueF, _ = utilities.GuessVar(CNF, Value, res, index, -1)
        if len(ValueF) == 0:
            break
        inputArray.append(constructMatrix(ValueT, CNFT))
        inputArray.append(constructMatrix(ValueF, CNFF))
        numpy_input_fn = tf.estimator.inputs.numpy_input_fn(x={"CNF":np.array(inputArray)}, shuffle = False)
        mod1Labels = model.predict(input_fn = numpy_input_fn)
        mod1Labels = list(mod1Labels)
        if mod1Labels[0] == 1:
            res[index] = 1
            CNF = CNFT
            Value = ValueT
        elif mod1Labels[1] == 1:
            CNF = CNFF
            Value = ValueF
            res[index] = -1
        else:
            break

        index += 1

    i = 1
    return res
    while i <= len(CNFs):
        index = len(CNFs) - i
        VerifiedRes = utilities.buildFormula(CNFs[index], Values[index])
        if VerifiedRes != "UNSAT":
            break
        i *= 2
    if VerifiedRes != "UNSAT":
        return VerifiedRes
        # concatRes is wrong when i is 1, len(BackRes[0]) is 129 but len(verifiedRes) is 128
        res = concatRes(BackRes[index], VerifiedRes)
        return res
    else:
        return []


def realWorldTrain(args):
    global length, width, batch_size
    #length =
    config = tf.ConfigProto()
    config.gpu_options.allow_growth = True
    modConf = tf.estimator.RunConfig(session_config = config)

    model = tf.estimator.Estimator(model_fn)

    for i in range(100):
        model.train(input_fn = lambda: my_input_fn(args[0], length, width, batch_size), steps=num_steps)

        # Evaluate the Model
        # Use the Estimator 'evaluate' method

        #model_path = model.export_savedmodel(export_dir, serving_input_receiver_fn)
        #model = tf.estimator.Estimator(model_fn, model_dir=model_path)

        begin = datetime.datetime.now()
        e = model.evaluate(input_fn = lambda: my_input_fn(args[0]+".tests", length, width, batch_size))
        end = datetime.datetime.now()
        k = end - begin
        print("Testing Accuracy for model 1:%f, in %f seconds" % (e['accuracy'], k.total_seconds()))
        print(e)

    for i in range(1, len(args)):
        begin = datetime.datetime.now()
        e = model.evaluate(input_fn = lambda: my_input_fn(args[i], length, width, batch_size))
        end = datetime.datetime.now()
        k = end - begin
        print("Testing Accuracy for file %s is :%f, in %f seconds" % (args[i], e['accuracy'], k.total_seconds()))
    exit()

    #mod2Width, mode2Length = load_data.loadInfo(args[1])
    count = 0
    predRes = []
    curRes = 1
    labels = []
    inputArray = []
    begin = datetime.datetime.now()
    for serialized_example in tf.python_io.tf_record_iterator(args[1]):
        example = tf.train.Example()
        example.ParseFromString(serialized_example)
        label = example.features.feature['label'].int64_list.value[0]
        index = example.features.feature['index'].int64_list.value[0]
        inputArray.append(parseOneRecord(example))
        count += 1
        if count == sampleNum:
            numpy_input_fn = tf.estimator.inputs.numpy_input_fn(x={"CNF":np.array(inputArray)}, shuffle = False)
            mod1Labels = model.predict(input_fn = numpy_input_fn)
            mod1Labels = list(mod1Labels)
            count = 0
            inputArray = []
            if 0 in mod1Labels:
                predRes.append(0)
            else:
                predRes.append(1)
            labels.append(label)

    end = datetime.datetime.now()
    k = end - begin
    evalPred(predRes, labels, k.total_seconds())
    exit()
    '''
    begin = datetime.datetime.now()
    mod1Labels = model.evaluate(input_fn = lambda: test_input_fn(args[1], length, width, batch_size))
    print(mod1Labels)
    exit()
    for lab in mod1Labels:
        if lab == 0:
            curRes = 0
        count += 1
        if count == sampleNum:
            predRes.append(curRes)
            curRes = 1
            count = 0
    end = datetime.datetime.now()
    k = end - begin
    evalPred(predRes, testLabels, k.total_seconds())
    '''

def solving(args):
    global length, width, batch_size, callCounter

    model = tf.estimator.Estimator(model_fn)

    model.train(input_fn = lambda: my_input_fn(args[0], length, width, batch_size), steps=num_steps)

    begin = datetime.datetime.now()
    e = model.evaluate(input_fn = lambda: my_input_fn(args[0]+".tests", length, width, batch_size))
    end = datetime.datetime.now()
    k = end - begin
    print("Testing Accuracy for model 1:%f, in %f seconds" % (e['accuracy'], k.total_seconds()))
    if isUseExtentDS:
        ESMName = export_dir + "/"+ os.path.splitext(args[0])[0]+"_ESM"
        predict_fn = tf.contrib.predictor.from_saved_model(ESMName)
    else:
        predict_fn = tf.contrib.predictor.from_estimator(model, serving_input_receiver_fn)

    begin = datetime.datetime.now()

    predRes = []
    labels = []
    for serialized_example in tf.python_io.tf_record_iterator(args[1]):
        inputArray = []
        example = tf.train.Example()
        example.ParseFromString(serialized_example)
        label = example.features.feature['label'].int64_list.value[0]
        labels.append(label)

        Value, CNF, inputShape = extractSparseMatrix(example)
        inputArray.append(parseOneRecord(example))

        numpy_input_fn = tf.estimator.inputs.numpy_input_fn(x={"CNF":np.array(inputArray)}, shuffle = False)
        mod1Labels = model.predict(input_fn = numpy_input_fn)
        callCounter += 1
        mod1Labels = list(mod1Labels)[0]
        if 1 == mod1Labels:
            if isFlat:
                res = predictCNFValuesFlat(CNF, Value, inputShape, predict_fn)
            else:
                res = predictCNFValues(CNF, Value, inputShape, model)
            if len(res) == 0:
                predRes.append(0)
            else:
                predRes.append(1)
        else:
            predRes.append(0)
    end = datetime.datetime.now()
    if flatPredRes[1] == 0:
        flatPredRes[1] = -1

    print("guess in %f, %f, %f, %f seconds, flatPredRes: %f"%
            (pck[0],pck[1], pck[2], pck[3], float(flatPredRes[0])/flatPredRes[1]))
    print("flatPredRes: "+str(flatPredRes))
    print(flatPredClass)
    print(callCounter)
    k = end - begin
    evalPred(predRes, labels, k.total_seconds())

def runMultiModels(args, trainSetNum):
    global length, width, batch_size, firstLayerKernel

    models = []
    for i in range(trainSetNum):
        width, length, batch_size, firstLayerKernel = retrieveBasicInfo(i)

        model = tf.estimator.Estimator(model_fn)
        model.train(input_fn = lambda: my_input_fn(args[i], length, width, batch_size), steps=num_steps)
        begin = datetime.datetime.now()
        e = model.evaluate(input_fn = lambda: my_input_fn(args[i]+".tests", length, width, batch_size))
        end = datetime.datetime.now()
        k = end - begin
        models.append(model)
        print("Testing Accuracy for model %d:%f, in %f seconds" % (i, e['accuracy'], k.total_seconds()))

    #mod2Width, mode2Length = load_data.loadInfo(args[1])
    predRes = []
    curRes = 1
    labels = []
    begin = datetime.datetime.now()
    for serialized_example in tf.python_io.tf_record_iterator(args[-1]):
        inputArray = []
        resCount = [0,0]
        example = tf.train.Example()
        example.ParseFromString(serialized_example)
        label = example.features.feature['label'].int64_list.value[0]
        index = example.features.feature['index'].int64_list.value[0]
        labels.append(label)
        for i in range(len(models)):
            width, length, batch_size, firstLayerKernel = retrieveBasicInfo(i)
            inputArray.append(parseOneRecord(example))
            numpy_input_fn = tf.estimator.inputs.numpy_input_fn(x={"CNF":np.array(inputArray)}, shuffle = False)
            mod1Labels = models[i].predict(input_fn = numpy_input_fn)
            mod1Labels = list(mod1Labels)[0]
            resCount[mod1Labels] += 1
            inputArray = []
        if resCount[0] > resCount[1]:
            predRes.append(0)
        else:
            predRes.append(1)

    end = datetime.datetime.now()
    k = end - begin
    evalPred(predRes, labels, k.total_seconds())

def analyzeBasicInfo(files, trainSetNum, argWidth, argLength, targetShape):
    smallBatchSize = 0
    for i in range(trainSetNum):
        loadWidth, loadLength = load_data.loadInfo(files[i])
        if argWidth != 0:
            if argWidth > loadWidth:
                loadWidth = argWidth
        if argLength != 0:
            if argLength > loadLength:
                loadLength = argLength

        batch_size = int(1024*1024/(loadWidth * loadLength))
        if loadWidth < 30:
            loadWidth = 30

        targetLen = max(1, int(loadLength / targetShape))
        targetWid = max(1, int(loadWidth / targetShape))
        firstLayerKernel = (targetLen, targetWid)
        if batch_size == 0:
            batch_size = 1
        if smallBatchSize == 0 or smallBatchSize > batch_size:
            smallBatchSize = batch_size

        basicInfoList.append([loadWidth, loadLength, batch_size, firstLayerKernel])
    for i in range(trainSetNum):
        basicInfoList[i][2] = smallBatchSize

def retrieveBasicInfo(i):
    #width, length, batch_size, firstLayerKernel
    return basicInfoList[i][0], basicInfoList[i][1], basicInfoList[i][2], basicInfoList[i][3]

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('files', metavar='N', type=str, nargs='+',
            help='an integer for the accumulator')
    parser.add_argument('--width', type = int, default=0)
    parser.add_argument('--length', type = int, default=0)
    parser.add_argument('--sampleNum', type = int,default=1)
    parser.add_argument('--trainSetNum', type = int,default=1)
    parser.add_argument('--targetShape', type = int,default=100)
    # 0 means false, 1 means directly solve, 2 means use improved method.
    parser.add_argument('--isFlat', type = int,default=0)
    parser.add_argument('--solve', type = int,default=0)
    parser.add_argument('--visualize', type = bool,default=False)
    parser.add_argument('--isUseExtentDS', type = int,default=0)
    parser.add_argument('--isSaveModel', type = int,default=0)
    args = parser.parse_args()
    files = args.files
    isUseExtentDS = args.isUseExtentDS
    isFlat = args.isFlat
    isSaveModel = args.isSaveModel
    sampleNum = args.sampleNum
    trainSetNum = args.trainSetNum
    isVisualize = args.visualize
    solveMethod = args.solve

    analyzeBasicInfo(files, trainSetNum, args.width, args.length, args.targetShape)
    width, length, batch_size, firstLayerKernel = retrieveBasicInfo(0)

    for i in range(trainSetNum):
        print("kernel size: (%d, %d), batch_size: %d" % (basicInfoList[i][3][0], basicInfoList[i][3][1], basicInfoList[i][2]))

    if trainSetNum != 1:
        runMultiModels(files, trainSetNum)
        exit()

    if solveMethod and len(files) > 1:
        solving(files)
        exit()

    if len(files) == 1:
        run_tfr(files)
    elif len(files) > 1:
        realWorldTrain(files)


