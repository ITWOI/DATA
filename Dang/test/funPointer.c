#include<stdio.h>
int max(int x,int y){return (x>y? x:y);}
int main()
{
        int (*ptr)(int, int);
        int (**ptr2)(int, int);
        ptr2 = &ptr;
        int a = 2, b = 1, c;
        ptr = max;
        c = (**ptr2)(a,b);
        printf("a=%d, b=%d, max=%d", a, b, c);
        return 0;
}
