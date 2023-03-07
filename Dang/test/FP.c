#include  <string.h> 
#include <stdio.h>
#include <stdlib.h>

void funPointer1()
{
    char *p1, *p2;
    p1 = malloc(16);
    p2 = p1+3;
    free(p1);
    *p2 = '2';
}

void funPointer2()
{
    char *p1, *p2;
    char **p1_1 = &p1, **p2_1 = &p2;
    *p1_1 = malloc(16);
    p2 = p1+3;
    p2_1 = &(*p1_1+3);
    free(*p1_1);
    *p1_1 = NULL;
    **p2_1 = '2';
}


int main()
{
        funPointer1();
        funPointer2();
        return 0;
}
