#include  <string.h> 
#include <stdio.h>
#include <stdlib.h>

char **g_p;

void strFun(char ** p)
{
    strcat(*p,"aa");
}
void funPointer()
{
    char *p_1=(char *)malloc(10*sizeof(char));
    char **p2, **p1;
    p1 = &p_1;
    strcpy(*p1, "hello");
    p2 = p1;
    strcpy(*p2, "world");
    strFun(p2);
    free(*p1);
    *p1=NULL;
    if(*p2 == NULL)
        printf("1\n");
    else
        printf("0\n");
}

void getAddr()
{
    //ignore
    int i = 0;
    int *p=&i;
    *p=2;
}

void getPointerAddr()
{
    //how to deal with p2?
    char *p_1=(char *)malloc(10*sizeof(char));
    char **p1 = &p_1;
    char ***p2 = &p1;
    strFun(*p2);
}

void strFun_1(char * p)
{
    strcat(p,"aa");
}
void arrayPointer()
{
        //p1 can't be modified, strFun can't too
        char p1[10];
        strFun_1(p1);
}

int strFun2(char p)
{
    if(p=='1')
        return 1;
    else
        return 0;
}  
   
void notMod()
{   
    char *p_1=(char *)malloc(10*sizeof(char));
    char **p1=&p_1;
    char ***p2 = &p1;
    int i = strFun2(**p1);
    printf("%d\n",i);
    i = strFun2(***p2);
    printf("%d\n",i);
}

char **pointRet()
{
    char *p_1 = (char *)malloc(10*sizeof(char));
    char **p = &p_1;
    return p;
}

void testRetnGlobal()
{
    g_p = pointRet();
    char **p1=g_p;
    strcpy(*p1,"aaaaaaa");
}


int main()
{
    funPointer();
    getAddr();
    getPointerAddr();
    arrayPointer();
    notMod();
    return 0;
}
