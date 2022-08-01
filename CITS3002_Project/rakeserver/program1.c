#include <stdio.h>
#include <stdlib.h>

extern int func1(int i);

int main(int argc, char **argv)
{
    printf("%i\n", 0 + func1(1));
    exit(0);
}
