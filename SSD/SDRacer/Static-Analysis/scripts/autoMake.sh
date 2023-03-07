#!/bin/bash

#cat simpleResgood.txt | awk -F ',' 'BEGIN{} {print $1;} END{}'
rm insmod.txt
cat simpleResgood.txt| while read myline
do 
    eval $(echo $myline | awk -F ',' {'printf("res=%s",$1)'})
    
    echo $res
    cp $res ./driver.c
    make
    echo "fff000" | sudo -S insmod driver.ko
    if [ $? = 0 ]
    then
        echo "fff000" | sudo -S rmmod driver.ko
        echo $myline >> insmod.txt
    fi
done
