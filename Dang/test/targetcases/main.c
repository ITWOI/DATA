#include  <string.h> 
#include <stdio.h>
#include <stdlib.h>
#include "other.h"

char * g_p = NULL;

//From llvm langref
struct RT {
    char A;
    char* B[10][20];
    char C;
};
struct ST {
    int X;
    double Y;
    struct RT Z;
};

char *foo(struct ST *s) {
    s = (struct ST *)malloc(sizeof(struct ST)*2);
    for(int i = 0; i< 10;i++)
    {
        for(int j = 0;j<20;j++)
        {
            s[1].Z.B[i][j] = (char*)malloc(10);
        }
    }
    return s[1].Z.B[0][0];
}

void testComp()
{
    struct ST *s;
    foo(s);
}


//from bzip2
typedef struct test
{
    int num;
    char* name;
    struct test *next;
}node;

void listtest()
{
    node* root = (node*)malloc(sizeof(node));
    node* cur = root;
    root->name = (char*)malloc(10);
    for(int i = 0; i<5; i++)
    {
        node* tmp = (node*)malloc(sizeof(node));
        cur->next = tmp;
        cur=tmp;
    }
    free(root->name);
    free(root);
}

int main()
{
    structTest();
    structGTest();
    structPTest();
    arrayTest();
    testComp();
    return 0;
}
