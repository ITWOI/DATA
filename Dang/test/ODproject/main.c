#include  <string.h> 
#include <stdio.h>
#include <stdlib.h>
#include "other.h"

char * g_p = NULL;

static
void* default_bzalloc ( void* opaque, int items, int size )
{
    void* v = malloc ( items * size );
    return v;
}

void funPointer()
{
    int n = 0;
    char *p1=(char *)malloc(10*sizeof(char));
    strcpy(p1, "hello");
    EP = p1;
    strcpy(EP, "world");
    strFun(EP);
    *p1=*EP;
    free(p1);
    p1=NULL;
    if(EP == NULL)
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
    *p2 = NULL;
	if(p1 == NULL)
        printf("1\n");
    else
        printf("0\n");
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

void bziptest()
{
    char *p2 = NULL;
    p2 = default_bzalloc(NULL, 1, 10);
    char *p1=p2;
    strcpy(p1,"aaaaaaa");
	free(p2);
    p2 = NULL;
	if(p1 == NULL)
        printf("1\n");
    else
        printf("0\n");
}

int main()
{
    funPointer();
    getPointerAddr();
    arrayPointer();
    notMod();
    testRetnGlobal();
    bziptest();

    getAddr();
    structTest();
    structGTest();
    structPTest();
    arrayTest();
    arrayGTest();
    return 0;
}
