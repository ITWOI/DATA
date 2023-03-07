#include  <string.h> 
#include <stdio.h>
#include <stdlib.h>

void strFun(char ** p)
{
    strcat(*p,"aa");
}

int main()
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
    return 0;
}
