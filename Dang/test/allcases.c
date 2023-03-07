#include  <string.h> 
#include <stdio.h>
#include <stdlib.h>
static char* gp=NULL;

void strFun(char * p)
{
    strcat(p,"aa");
}

void rule1()
{
    char *p1=(char *)malloc(10*sizeof(char));
    char *p2;
    strcpy(p1, "hello");
    p2 = p1;
    gp = p1;
    strcpy(p2, "world");
    strFun(p2);
    free(p1);
    p1=NULL;
    if(p2 == NULL)
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

void testFun(char *p)
{
    strcat(p,"aa");
}

void rule2_3()
{
    //how to deal with p2?
    char *p1=(char *)malloc(10*sizeof(char));
    char **p2 = &p1;
    strFun(*p2);
    testFun(*p2);
}

void rule4()
{
    //p1 can't be modified, strFun can't too
    char p1[10];
    strFun(p1);
}
int strFun2(char p)
{
    if(p=='1')
        return 1;
    else
        return 0;
}

void rule2_3_1()//same rule as rule 2_3
{
    char *p1=(char *)malloc(10*sizeof(char));
    char **p2 = &p1;
    int i = strFun2(*p1);
    printf("%d\n",i);
    i = strFun2(**p2);
    printf("%d\n",i);
}

char *test5()
{
    char *p1=(char *)malloc(10*sizeof(char));
    strcpy(p1, "world");
    return p1;
}

void rule5()
{
    char *p = test5();
    strcat(p, " hello");
}


int main()
{
    rule1();
    getAddr();
    rule4();
    rule2_3();
    rule2_3_1();
    rule5();
    return 0;
}
