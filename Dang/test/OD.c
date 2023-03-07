#include  <string.h> 
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void strFun(char * p)
{
    strcat(p,"aa");
}

void test()
{
    int i = 0;
    for(; i<100;i++)
    {
        char *p1=(char *)malloc(10*sizeof(char));
        char *p2;
        strcpy(p1, "hello");
        int j = 0;
        for(;j<100000;j++)
        {
            p2 = p1;
            *p1 = '1';
        }
        strcpy(p2, "world");
        strFun(p2);
        free(p1);
        p1=NULL;
    }

}
int main()
{
    int a = clock();
    test();
    double b = (double)(clock() - a)/CLOCKS_PER_SEC;
    printf("%f second\n", b);
    return 0;
}
