#!/bin/bash
SIMICS_PATH=/home/fff000/simics-workspace

if [ $# \< "1" ];then
        echo "Usage: $1 pdrfilename"
        exit -1
fi

for line in `python ./scripts/analysePDR.py $1` ; do
    echo $line

done
