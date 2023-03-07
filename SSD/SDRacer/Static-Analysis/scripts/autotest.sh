#!/bin/bash
rm res.csv
cat files.txt | while read myline
do 
    cp $myline ./driver.c
    cp Makefile_ori Makefile
    make
    rescmd=${myline}","$?
    cp Makefile_clang Makefile
    make NAME=driver
    rescmd=${rescmd}","$?
    echo $rescmd
    echo $rescmd >> res.csv
done
