#include <stdio.h>
#include <stdlib.h>

extern int func1(int i);
extern int func2(int i);
extern int func3(int i);
extern int func4(int i);
extern int func5(int i);
extern int func6(int i);
extern int func7(int i);
extern int func8(int i);
extern int func9(int i);
extern int func10(int i);

int main(int argc, char **argv)
{
    printf("%i\n", 0 + func1(1) + func2(2) + func3(3) + func4(4) + func5(5) + func6(6) + func7(7) + func8(8) + func9(9) + func10(10));
    exit(0);
}
