#!/bin/bash
echo "" > ./tmp.txt
while true
do
    top -b -n 1 -u fff000 -i >>./tmp.txt
    echo -e '\n' >> ./tmp.txt
    sleep 300
done
