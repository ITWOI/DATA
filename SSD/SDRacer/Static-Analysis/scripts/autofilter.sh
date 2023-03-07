#!/bin/bash
cat res.csv | awk -F ',' 'BEGIN{} {if($3==0 && $2==0){print $1;}} END{}' > good.txt
#cat res.csv | awk -F ',' '{print $1}'
