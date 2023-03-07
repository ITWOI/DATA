# Dataset of GINN

The link of this dataset is https://figshare.com/articles/dataset/datasets_tar_gz/8796677.

## Structure of the dataset

```
├── dataset
│   ├──bugs-dot-jar
│   ├──fse14-dataset
│   ├──BugSwarm
│   ├──defects4j
```


## Setup datasets.

I upload as much data as possible, but due to the space limit of figshare, I didn't upload git repos required for `BugSwarm`.
To download missing data in `BugSwarm`, run `generateDataset.py` and move git repos to `dataset/BugSwarm/gitrepos`.

## Entry of the datasets.

Each folder contains a shell script named as `autoRun.sh`, users should open it and change the output dir. For example, change `callGenJson dotjar /proj/fff000/ggnnbl/spoon-intervals/jsondata-inline0/ -inline 0` to `callGenJson dotjar /path/to/your/output/dir -inline 0`. `inline` argument means how many context we include.
