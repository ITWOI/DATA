#include  <string.h> 
#include <stdio.h>
#include <stdlib.h>

void funPointer1()
{
    char *p1;
    long p2;
    p1 = malloc(16);
    p2 = (long)p1;
    free(p1);
    *(char*)p2 = '2';
    printf("%c\n", *(char*)p2);
}

void funPointer2()
{
    char *p1;
    char** p1_1 = &p1;
    long p2;
    long *p2_1 = &p2;
    p1 = malloc(16);
    p2 = (long)p1;
    p2_1 = (long *)p1_1;
    free(*p1_1);
    *p1_1 = NULL;
    **(char**)p2_1 = '2';
    printf("%c\n", **(char**)p2_1);

}


int main()
{
        funPointer1();
        funPointer2();
        return 0;
}
