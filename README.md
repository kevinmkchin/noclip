# noclip

Single header C++11 library for parsing and interpreting commands
and arguments from an input stream. Can be used to make REPL-like
environments e.g. drop-down console.

Type information is baked into the anonymous functions for setting/getting variables
and invoking commands. Type information is used to reify arguments at execution time.

For input and output, `noclip.h` uses `std::istream` and `std::ostream`. This
makes parsing of custom types possible by overloading the insertion and extraction
operators.

![image](https://github.com/user-attachments/assets/21ff12f1-d77e-498c-a0df-af2c8d87c912)

### Console Commands
```c++
void set_cheats(int mode) { ... }
...
console.bind_cmd("sv_cheats", set_cheats);
console.execute("sv_cheats 1", std::cout); // calls set_cheats with mode 1
```

### Console Variables
```c++
int hp = 100;
console.bind_cvar("health", &hp);
console.execute("set health 99", std::cout);
console.execute("get health", std::cout); // prints value of hp
```

### Creating the Console
```c++
#include "noclip.h"
noclip::console c; // default constructor
```
More documentation included directly in the header file.
