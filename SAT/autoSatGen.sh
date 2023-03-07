#!/bin/bash
MaxNum=4000
name=1
path=$1
filePath=$path/$name.cnf
while [ -f "$filePath" ]
do
    let name++
    filePath=$path/$name.cnf
done
count=0
thread=8
for((i=0;i<$MaxNum;i++))
do
    varNum=$(($(($RANDOM % 50)) + 400))
    clauseRatio=$(($(($RANDOM % 5)) + 3))
    clause=$(($varNum * $clauseRatio))
    cnfgen randkcnf 3 $varNum $clause > $path/$name.cnf &
    let name++
    let count++
    b=$(( $count % $thread ))
    if [ $b = 0 ] ; then
        wait
    fi
    filePath=$path/$name.cnf
done
