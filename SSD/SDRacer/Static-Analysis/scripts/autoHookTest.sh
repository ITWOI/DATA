#!/bin/bash
rm res.csv
cp Makefile_ori Makefile

cat $1 | while read myline
do 
    cp $myline ./driver.c
    echo ${myline%%.*}".h" ./
    rm *.ast
    make
    rescmd=${myline}","$?
    make CC=/home/fff000/Documents/test/kernelTest/klee-gcc
    if [ -s driver.ast ]
    then
        rescmd=${rescmd}","0
    else
        rescmd=${rescmd}","2
    fi
    echo $rescmd
    echo $rescmd >> res.csv
done
