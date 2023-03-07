#include <string.h> 
#include <stdio.h>
#include <stdlib.h>
#include "other.h"

ts *Gtsp;
char *ap[10];
char **ap1[10];


void structTest()
{
    ts *tsp1 = (ts *)malloc(sizeof(ts));
    ts *tsp2;
    tsp2 = tsp1;
    free(tsp2);
    tsp2 = NULL;
    if(tsp1 == NULL)
        printf("1\n");
    else
        printf("0\n");
}

void structGTest()
{
    Gtsp = (ts *)malloc(sizeof(ts));
    ts *tsp2;
    tsp2 = Gtsp;
    free(tsp2);
    tsp2 = NULL;
    if(Gtsp == NULL)
        printf("1\n");
    else
        printf("0\n");
}


void structPTest()
{
/*
    ts tsp1;
    ts *tsp2 = (ts *)malloc(sizeof(ts));
    tsp1.buf = (char *)malloc(10*sizeof(char));
    strFun(tsp1.buf);
    tsp2->buf = tsp1.buf;
    free(tsp2->buf);
    tsp2->buf = NULL;
    if(tsp1.buf == NULL)
        printf("1\n");
    else
        printf("0\n");
*/
}
void arrayTest()
{
    ap[0]=(char *)malloc(10*sizeof(char));
    ap1[0] = &ap[0];
    strcpy(*ap1[0], "hello");
    ap1[1] = ap1[0];
    free(*ap1[0]);
    ap1[0]=NULL;
    if(ap1[1] == NULL)
        printf("1\n");
    else
        printf("0\n");
}
