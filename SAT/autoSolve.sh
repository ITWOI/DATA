#!/bin/bash
function getTiming() {
    start=$1
    end=$2

    start_s=$(echo $start | cut -d '.' -f 1)
    start_ns=$(echo $start | cut -d '.' -f 2)
    end_s=$(echo $end | cut -d '.' -f 1)
    end_ns=$(echo $end | cut -d '.' -f 2)

    time=$(( ( 10#$end_s - 10#$start_s ) * 1000 + ( 10#$end_ns / 1000000 - 10#$start_ns / 1000000 ) ))

    return "$time"
}

function solvingCNF()
{
    filename=`basename $2`
    if [ -f  $1/${filename%.*}.res ]
    then
        return
    fi
    start=$(date +%s.%N)
    res=$(timeout 3600 z3 $2)
    end=$(date +%s.%N)
    getTiming $start $end
    exeTime=$?
    echo "$res" > $1/${filename%.*}.res
    echo $exeTime >> $1/${filename%.*}.res
}

thread=4
targetNum=$(( $thread - 1 ))
count=0
threadArray=()

function callSolver()
{
    for data in ${threadArray[@]}
    do
        solvingCNF $1 ${data} &
    done
    wait
    threadArray=()
}

export -f solvingCNF
export -f getTiming

res=`find $1 -name "*.cnf"`

totalLine=`echo "$res" | wc -l`
echo "$res" | parallel --max-procs=5 solvingCNF $1
