#include  <string.h> 
#include <stdio.h>
#include <stdlib.h>
#include "other.h"

char * g_p = NULL;


void funPointer()
{
    char *p1=(char *)malloc(10*sizeof(char));
    char *p2;
    strcpy(p1, "hello");
    p2 = p1;
    strcpy(p2, "world");
    strFun(p2);
    *p1=*p2;
    free(p1);
    p1=NULL;
    if(p2 == NULL)
        printf("1\n");
    else
        printf("0\n");
}


void getPointerAddr()
{
    //how to deal with p2?
    char *p1=(char *)malloc(10*sizeof(char));
    char **p2 = &p1;
    strFun(*p2);
    free(*p2);
}

void arrayPointer()
{
    //p1 can't be modified, strFun can't too
    char p1[10];
    strFun(p1);
}

void notMod()
{
    char *p1=(char *)malloc(10*sizeof(char));
    char **p2 = &p1;
    int i = strFun2(*p1);
    i = strFun2(**p2);
    free(*p2);
}

char *pointRet()
{
    char *p = (char *)malloc(10*sizeof(char));
    return p;
}

void testRetnGlobal()
{
    g_p = pointRet();
    char *p1=g_p;
    strcpy(p1,"aaaaaaa");
	free(g_p);
    g_p = NULL;
	if(p1 == NULL)
        printf("1\n");
    else
        printf("0\n");
}

int main()
{
    funPointer();
    getAddr();
    getPointerAddr();
    arrayPointer();
    notMod();
    testRetnGlobal();

    getAddr();
    structTest();
    structGTest();
    structPTest();
    arrayTest();
    return 0;
}
