#include <stdio.h>

int main()
{
    int arg0 = 123456;
    int arg1 = 654321;

    // Phase one, push information to stack.
    asm volatile ("push %[c]\n\t"
                  "push %[w]\n\t"
                  "ud2\n\t"
                  "pop %[w]\n\t"
                  "pop %[c]"
                  : // No outputs
                  : [c] "r" (&arg0), [w] "r" (&arg1)
                  : "rsp");

    printf("%d\n", arg0);
    printf("%d\n", arg1);
}
