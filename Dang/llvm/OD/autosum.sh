#!/bin/bash
cat $1 | grep "Num of modified vars:" | awk '{sum+=$5;print $0,sum}'
cat $1 | grep "Num of modified instructions:" | awk '{sum+=$5;print $0,sum}'
cat $1 | grep "Num of total Pointers:" | awk '{sum+=$5;print $0,sum}'
