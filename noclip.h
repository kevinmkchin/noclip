/*

noclip.h

Single header C++11 library for parsing and interpreting commands
and arguments from an input stream. Can be used to make REPL-like
environments e.g. drop-down console.

Uses C++11 features (e.g. parameter pack, anonymous functions).

Type information is baked into the anonymous functions for setting/getting variables
and invoking commands. Type information is used to reify arguments at execution time.

For input and output, noclip::console uses std::istream and std::ostream. This
makes parsing of custom types possible by overloading the insertion and extraction
operators.

USAGE:
    function    | example
    ------------|-----------------------------------------
    bind_cvar   | console.bind_cvar("health", &health);
                |
    bind_cmd    | console.bind_cmd("command", someFunction);
                | console.bind_cmd("command", &Object::memberFunc, &objectInstance);
                | console.bind_cmd("command", [](std::istream& is, std::ostream& os){ lambda body });
                |
    unbind_cvar | console.unbind_cvar("health"); // useful if 'health' goes out of scope (i.e. dealloc'ed)
                |
    unbind_cmd  | console.unbind_cvar("command"); // useful if object owning 'command' goes out of scope
                |
    execute     | console.execute(std::cin, std::cout);
                | console.execute("set health 99", std::cout);

CREATING A CONSOLE:
    noclip::console console;

*/
#ifndef NOCLIP_CONSOLE_H
#define NOCLIP_CONSOLE_H

#include <iostream>
#include <sstream>
#include <functional>
#include <map>

#ifndef NOCLIP_MULTIPLE_COMMANDS_DELIMITER
#define NOCLIP_MULTIPLE_COMMANDS_DELIMITER ';'
#endif // NOCLIP_MULTIPLE_COMMANDS_DELIMITER

namespace noclip
{
    typedef std::function<void(std::istream& is, std::ostream& os)> console_function_t;

    struct console
    {
        console()
        {
            bind_builtin_commands();
        }

        typedef std::map<std::string, console_function_t> function_table_t;
        function_table_t cmd_table;
        function_table_t cvar_setter_lambdas;
        function_table_t cvar_getter_lambdas;

        template<typename T>
        void bind_cvar(const std::string& vid, T* vmem)
        {
            cvar_setter_lambdas[vid] =
                [this, vid, vmem](std::istream& is, std::ostream& os)
                {
                    T read = this->evaluate_argument<T>(is, os);

                    if(is.fail())
                    {
                        const char* vt = typeid(T).name();
                        os << "CONSOLE ERROR: Type mismatch. CVar '" << vid 
                        << "' is of type '" << vt << "'." << std::endl;

                        is.clear();
                    }
                    else
                    {
                        *vmem = read;
                    }
                };

            cvar_getter_lambdas[vid] =
                [vmem](std::istream& is, std::ostream& os)
                {
                    os << *vmem << std::endl;
                };
        }

        template<typename ... Args>
        void bind_cmd(const std::string& cid, void(*f_ptr)(Args ...))
        {
            cmd_table[cid] = 
                [this, f_ptr](std::istream& is, std::ostream& os)
                {
                    auto std_fp = std::function<void(Args ...)>(f_ptr);
                    this->reify_and_execute<Args ...>(is, os, std_fp);
                };
        }

        /* e.g. bind_cmd("myObjectMethod", &MyObject::f, &myObjectInstance) */
        template<typename T, typename ... Args>
        void bind_cmd(const std::string& cid, void(T::*f_ptr)(Args ...), T* omem)
        {
            std::function<void(Args...)> std_fp =
                [f_ptr, omem](Args ... args)
                {
                    (omem->*f_ptr)(args...); // could use std::mem_fn instead
                };

            cmd_table[cid] =
                [this, std_fp](std::istream &is, std::ostream &os)
                {
                    this->reify_and_execute<Args...>(is, os, std_fp);
                };
        }

        void bind_cmd(const std::string& cid, console_function_t iofunc)
        {
            cmd_table[cid] = iofunc;
        }

        void unbind_cvar(const std::string& vid)
        {
            auto v_set_iter = cvar_setter_lambdas.find(vid);
            if(v_set_iter != cvar_setter_lambdas.end())
            {
                cvar_setter_lambdas.erase(v_set_iter);
            }

            auto v_get_iter = cvar_getter_lambdas.find(vid);
            if(v_get_iter != cvar_getter_lambdas.end())
            {
                cvar_getter_lambdas.erase(v_get_iter);
            }
        }

        void unbind_cmd(const std::string& cid)
        {
            auto cmd_iter = cmd_table.find(cid);
            if(cmd_iter != cmd_table.end())
            {
                cmd_table.erase(cmd_iter);
            }
        }

        void execute(std::istream& input, std::ostream& output)
        {
            while (!input.eof())
            {
                std::string cmd;

                for (char c = 0; !input.eof(); c = 0)
                {
                    input.get(c);
                    if (c == 0 || c == NOCLIP_MULTIPLE_COMMANDS_DELIMITER)
                        break;
                    cmd.push_back(c);
                }

                if (!cmd.empty())
                {
                    std::stringstream cmdstream;
                    cmdstream.str(cmd);

                    std::string cmd_id;
                    cmdstream >> cmd_id;

                    auto cmd_iter = cmd_table.find(cmd_id);
                    if (cmd_iter == cmd_table.end())
                    {
                        output << "CONSOLE ERROR: Input '" << cmd_id << "' isn't a command." << std::endl;
                        return;
                    }

                    (cmd_iter->second)(cmdstream, output);
                }
            }
        }

        void execute(const std::string& str, std::ostream& output)
        {
            std::stringstream cmdstream;
            cmdstream.str(str);
            execute(cmdstream, output);
        }

    private:
        void read_arg(std::istream& is)
        {
            /* base case */
        }

        template<typename T, typename ... Ts>
        void read_arg(std::istream& is, T& first, Ts &... rest)
        {
            /*  Variadic template that recursively iterates each function 
                argument type. For each arg type, parse the argument and
                set value of first. First is a reference to a parameter 
                of read_args_and_execute. */

            std::ostringstream discard;
            first = evaluate_argument<T>(is, discard);
            read_arg(is, rest ...);
        }

        template <typename ... Args>
        void read_args_and_execute(std::istream& is, std::ostream& os, std::function<void(Args ...)> f_ptr, 
            typename std::remove_const<typename std::remove_reference<Args>::type>::type ... temps)
        {
            read_arg(is, temps...);

            if(is.fail())
            {
                is.clear();
                os << "CONSOLE ERROR: Incorrect argument types." << std::endl;
                return;
            }
            f_ptr(temps...);
        }

        template<typename ... Args>
        void reify_and_execute(std::istream& is, std::ostream& os, std::function<void(Args ...)> f_ptr)
        {
            read_args_and_execute(is, os, f_ptr,
                (reify<typename std::remove_const<typename std::remove_reference<Args>::type>::type>())...);
        }

        template<typename T>
        T reify()
        {
            /*  This function is to materialize the arbitrary number of 
                types into an arbitrary number of rvalues of those types. */
            return T();
        }

        template<typename T>
        T evaluate_argument(std::istream& is, std::ostream& os)
        {
            /*  Evaluate argument expressions e.g. set x (+ 3 7) */

            while(isspace(is.peek()))
            {
                is.ignore();
            }

            if(is.peek() == '(')
            {
                is.ignore(); // '('
                const int max_argument_size = 256;
                char argument_buffer[max_argument_size];
                is.get(argument_buffer, max_argument_size, ')');
                is.ignore(); // ')'

                std::ostringstream result;
                execute(std::string(argument_buffer), result);

                T read;
                std::istringstream(result.str()) >> read;
                return read;
            }
            else
            {
                T read;
                is >> read;
                return read;
            }
        }

        void bind_builtin_commands()
        {
            cmd_table["set"] = 
                [this](std::istream& is, std::ostream& os)
                {
                    std::string vid;
                    is >> vid;
                    auto v_iter = cvar_setter_lambdas.find(vid);
                    if(v_iter == cvar_setter_lambdas.end())
                    {
                        os << "CONSOLE ERROR: There is no bound variable with id '" << vid << "'." << std::endl;
                        return;
                    }
                    else
                    {
                        (v_iter->second)(is, os);
                    }
                };

            cmd_table["get"] =
                [this](std::istream& is, std::ostream& os)
                {
                    std::string vid;
                    is >> vid;
                    auto v_iter = cvar_getter_lambdas.find(vid);
                    if(v_iter == cvar_getter_lambdas.end())
                    {
                        os << "CONSOLE ERROR: There is no bound variable with id '" << vid << "'." << std::endl;
                        return;
                    }
                    else
                    {
                        (v_iter->second)(is, os);
                    }
                };

            cmd_table["help"] =
                [](std::istream& is, std::ostream& os)
                {
                    os << std::endl;
                    os << "-- Console Help --" << std::endl;
                    // os << "Set and get bound variables with" << std::endl;
                    os << "    set <cvar id> <value>" << std::endl;
                    os << "    get <cvar id>" << std::endl;
                    // os << std::endl;
                    // os << "Call bound and compiled C++ functions with" << std::endl;
                    // os << "<cmd id> <arg 0> <arg 1> ... <arg n>" << std::endl;
                    // os << std::endl;
                    // os << "Get help" << std::endl;
                    os << "    help : outputs help message" << std::endl;
                    os << "    cvars : list bound console variables" << std::endl;
                    os << "    procs : list bound console commands" << std::endl;
                    os << std::endl;
                    // os << "Perform arithematic and modulo operations" << std::endl;
                    // os << "(+, -, *, /, %) <lhs> <rhs>" << std::endl;
                    // os << std::endl;
                    // os << "You can pass expressions as arguments" << std::endl;
                    // os << "+ (- 3 2) (* 4 5)" << std::endl;
                    // os << "set x (get y)" << std::endl;
                    // os << "------------" << std::endl;
                };

            cmd_table["cvars"] =
                [this](std::istream& is, std::ostream& os)
                {
                    if(cvar_getter_lambdas.size() == 0)
                    {
                        os << "There are no bound console variables..." << std::endl;
                        return;
                    }

                    os << std::endl;
                    for (auto& it: cvar_getter_lambdas)
                    {
                        os << "    " << it.first << std::endl;
                    }
                    os << std::endl;
                };

            cmd_table["procs"] =
                [this](std::istream& is, std::ostream& os)
                {
                    if(cmd_table.size() == 0)
                    {
                        os << "There are no bound console commands..." << std::endl;
                        return;
                    }

                    os << std::endl;
                    for (auto& it : cmd_table)
                    {
                        if (it.first == "+" || it.first == "-" || it.first == "*" || it.first == "/" 
                            || it.first == "%" || it.first == "set" || it.first == "get" || it.first == "help" 
                            || it.first == "procs" || it.first == "cvars") continue;

                        os << "    " << it.first << std::endl;
                    }
                    os << std::endl;
                };

            cmd_table["+"] =
                [this](std::istream& is, std::ostream& os)
                {
                    float a = this->evaluate_argument<float>(is, os);
                    float b = this->evaluate_argument<float>(is, os);
                    os << a + b << std::endl;
                };

            cmd_table["-"] =
                [this](std::istream& is, std::ostream& os)
                {
                    float a = this->evaluate_argument<float>(is, os);
                    float b = this->evaluate_argument<float>(is, os);
                    os << a - b << std::endl;
                };

            cmd_table["*"] =
                [this](std::istream& is, std::ostream& os)
                {
                    float a = this->evaluate_argument<float>(is, os);
                    float b = this->evaluate_argument<float>(is, os);
                    os << a * b << std::endl;
                };

            cmd_table["/"] =
                [this](std::istream& is, std::ostream& os)
                {
                    float a = this->evaluate_argument<float>(is, os);
                    float b = this->evaluate_argument<float>(is, os);
                    os << a / b << std::endl;
                };

            cmd_table["%"] =
                [this](std::istream& is, std::ostream& os)
                {
                    int a = this->evaluate_argument<int>(is, os);
                    int b = this->evaluate_argument<int>(is, os);
                    os << a % b << std::endl;
                };
        }
    };
}


/*
------------------------------------------------------------------------------
This software is dedicated to Public Domain (www.unlicense.org)

This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/

#endif //NOCLIP_CONSOLE_H
