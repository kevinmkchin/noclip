#include "../noclip.h"

struct A
{
    float x = 3.1415;

    void f(int i)
    {
        std::cout << "A::f + " << x << std::endl;
    }
};

void funcwithargs(const std::string& str, const int& i)
{
    std::cout << "called it" << str << i << std::endl;
}

int fib(int n)
{
    if (n <= 1)
        return n;
    return fib(n-1) + fib(n-2);
}

int main()
{
    int i;
    float f;
    std::string s;

    noclip::console c;
    c.bind_cvar("i", &i);
    c.bind_cvar("f", &f);
    c.bind_cvar("s", &s);
    c.bind_cmd("printstuff", printstuff);

    c.bind_cmd("fib", fib);

    A a;
    c.bind_cvar("ax", &a.x);
    c.bind_cmd("af", &A::f, &a);

    while(true)
    {
        c.execute(std::cin, std::cout);
    }

    return 0;
}
