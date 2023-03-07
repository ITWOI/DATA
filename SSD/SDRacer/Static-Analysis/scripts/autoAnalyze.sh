#!/bin/bash
rm simpleRes$1
rm errorRes$1
#cat good.txt | while read myline
cat $1 | while read myline
do 
    rm GuidedPath.txt
    cp $myline ./driver.c
    make CC=/home/fff000/Documents/test/kernelTest/klee-gcc
    outp=$(SRacer astList.txt config.txt ./|wc -l)
    if [ $? = 0 ]
    then
        if [ -s GuidedPath.txt ]
        then
            outp=$myline","$outp","$(cat driver.c | wc -l)
            echo $outp >> simpleRes$1
        fi
    else
        echo $myline >> errorRes$1
    fi
done
