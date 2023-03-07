#include  <string.h> 
#include <stdio.h>
#include <stdlib.h>

char * g_p;

void strFun(char * p)
{
        strcat(p,"aa");
}

void funPointer1()
{
        char *p1=(char *)malloc(10*sizeof(char));
        char *p2;
        strcpy(p1, "hello");
        p2 = p1;
        strcpy(p2, "world");
        strFun(p2);
        free(p1);
        p1=NULL;
        if(p2 == NULL)
                printf("1\n");
        else
                printf("0\n");
}

void funPointer2()
{
        int n = 0;
        char *p1=(char *)malloc(10*sizeof(char));
        strcpy(p1, "hello");
        g_p = p1;
        strcpy(g_p, "world");
        strFun(g_p);
        *p1=*g_p;
        free(p1);
        p1=NULL;
        if(g_p == NULL)
                printf("1\n");
        else
                printf("0\n");
}

void funPointer3()
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

void notMod()
{
        char *p1=(char *)malloc(10*sizeof(char));
        char **p2 = &p1;
        strFun(p1);
        strFun(*p2);
        free(*p2);
        *p2 = NULL;
        if(p1 == NULL)
                printf("1\n");
        else
                printf("0\n");
}

int main()
{
        char *p1, *p2;
        p1 = (char *)malloc(32);
        strcpy(p1, "hello");
        p2 = p1;
        strcpy(p2, "world");
        free(p1);
        if(p2 == NULL)
                printf("1\n");
        else
                printf("0\n");
        funPointer1();
        funPointer2();
        funPointer3();
        notMod();
        return 0;
}
