#include <stdio.h>
#include <stdint.h>

#include <string>
#include <vector>
#include <map>

struct value {
    double real = 0;
    std::string text;
    bool is_number = true;
    
    value(double v)
    {
        real = v;
        is_number = true;
    }
    value(std::string t)
    {
        text = t;
        is_number = false;
    }
    value()
    {
    }
};

struct progstate
{
    uint64_t pc = 0;
    bool truth_register = false;
    std::vector<uint8_t> bytecode;
    std::vector<std::map<std::string, value>> variables;
    std::vector<std::vector<value>> stack;
    std::vector<uint64_t> stackdepths;
    
    void reset()
    {
        pc = 0;
        truth_register = false;
        variables.clear();
        stack.clear();
        stackdepths.clear();
    }
};

enum {
    NOP       = 0x00,
    PUSHVAL   = 0x01,
    PUSHTEXT  = 0x02,
    PUSHVAR   = 0x03,
    DECLARE   = 0x04,
    DECLSET   = 0x05,
    BINOP     = 0x06,
    UNOP      = 0x07,
    BINAS     = 0x08,
    UNAS      = 0x09,
    TRUTH     = 0x0A,
    OPENSCOPE = 0x10, // opens a new variable declaration table
    EXITSCOPE = 0x11, // closes current var dec table
    SAVESCOPE = 0x12, // saves the current depth we're on in the stack of var decl tables to a stack
    LOADSCOPE = 0x13, // goes to saved stack depth
    BREAK     = 0x14, // jumps to offset and goes to saved stack depth
    JSIT      = 0x15,
    JLIT      = 0x16,
    JSIF      = 0x17,
    JLIF      = 0x18,
    JS        = 0x19,
    JL        = 0x1A,
    CALL      = 0x1B, // note: uses a unique progstate if calling a user-defined function
    FUNCDEF   = 0x1C,
    RETURN    = 0x1D,
};

enum {
    ADD = 0x80,
    SUB = 0x81,
    MUL = 0x82,
    DIV = 0x83,
    EQ  = 0x84,
    NEQ = 0x85,
    GTE = 0x86,
    LTE = 0x87,
    GT  = 0x88,
    LT  = 0x89,
    AND = 0x8A,
    OR  = 0x8B
};

enum {
    POSITIVE = 0x80,
    NEGATIVE = 0x81,
    NEGATION = 0x82,
};

enum {
    ASSIGN = 0x80,
    MUTADD = 0x81,
    MUTSUB = 0x82,
    MUTMUL = 0x83,
    MUTDIV = 0x84,
};

enum {
    INCREMENT = 0x80,
    DECREMENT = 0x81,
};

void interpret(progstate * program)
{
    if(program == nullptr)
    {
        puts("Program is nullptr");
        return;
    }
    auto & pc = program->pc;
    auto & bytecode = program->bytecode;
    auto & variables = program->variables;
    auto & stack = program->stack;
    auto & truth_register = program->truth_register;
    variables.push_back({});
    stack.push_back({});
    auto * valstack = &(stack[0]);
    auto * varstack = &(variables[0]);
    auto & stackdepths = program->stackdepths;
    while(1)
    {
        if(pc == bytecode.size())
        {
            puts("Exited program");
            return;
        }
        //printf(">%08X\n", pc);
        if(pc > bytecode.size())
        {
            puts("Flew out of program");
            return;
        }
        auto loc = pc;
        auto opcode = bytecode[pc++];
        switch(opcode)
        {
        case NOP:
        {
            break;
        }
        case PUSHVAL:
        {
            uint8_t b1 = bytecode[pc++];
            uint8_t b2 = bytecode[pc++];
            uint8_t b3 = bytecode[pc++];
            uint8_t b4 = bytecode[pc++];
            uint8_t b5 = bytecode[pc++];
            uint8_t b6 = bytecode[pc++];
            uint8_t b7 = bytecode[pc++];
            uint8_t b8 = bytecode[pc++];
            // big endian
            uint64_t temp = 0;
            temp |= uint64_t(b1)<<(8*7);
            temp |= uint64_t(b2)<<(8*6);
            temp |= uint64_t(b3)<<(8*5);
            temp |= uint64_t(b4)<<(8*4);
            temp |= uint64_t(b5)<<(8*3);
            temp |= uint64_t(b6)<<(8*2);
            temp |= uint64_t(b7)<<(8*1);
            temp |= uint64_t(b8)<<(8*0);
            double value;
            memcpy(&value, &temp, sizeof(double));
            valstack->push_back(value);
            break;
        }
        case PUSHTEXT:
        {
            std::string text;
            uint8_t c = bytecode[pc++];
            while (c != 0)
            {
                text += c;
                c = bytecode[pc++];
            }
            
            valstack->push_back(text);
            break;
        }
        case PUSHVAR:
        {
            std::string name;
            uint8_t c = bytecode[pc++];
            while (c != 0)
            {
                name += c;
                c = bytecode[pc++];
            }
            
            if(variables.size() == 0)
            {
                puts("Internal error: no stack of variables");
                exit(0);
            }
            auto varstack_current = variables.size()-1;
            while(variables[varstack_current].count(name) == 0 and varstack_current > 0)
                varstack_current--;
            auto & vstack = variables[varstack_current];
            if(vstack.count(name))
            {
                valstack->push_back(vstack[name]);
            }
            else
            {
                puts("Error: access of undeclared variable");
                return;
            }
            
            break;
        }
        case DECLARE:
        {
            std::string name;
            uint8_t c = bytecode[pc++];
            while (c != 0)
            {
                name += c;
                c = bytecode[pc++];
            }
            if(varstack->count(name))
            {
                puts("Error: redeclaration");
                return;
            }
            else
            {
                (*varstack)[name] = 0;
            }
            
            break;
        }
        case DECLSET:
        {
            if(valstack->size() < 1)
            {
                puts("Error: not enough arguments to compound declaration");
                return;
            }
            std::string name;
            uint8_t c = bytecode[pc++];
            while (c != 0)
            {
                name += c;
                c = bytecode[pc++];
            }
            if(varstack->count(name))
            {
                puts("Error: redeclaration");
                return;
            }
            else if(valstack->size() < 1)
            {
                puts("Error: not enough arguments to declaration-assignment");
                return;
            }
            else
            {
                value right = valstack->back();
                valstack->pop_back();
                (*varstack)[name] = right;
            }
            
            break;
        }
        case BINOP:
        {
            if(valstack->size() < 2)
            {
                puts("Error: not enough arguments to binary operation");
                return;
            }
            value right = valstack->back();
            valstack->pop_back();
            value left = valstack->back();
            valstack->pop_back();
            
            if(right.is_number and left.is_number)
            {
                switch(bytecode[pc++])
                {
                case ADD:
                {
                    valstack->push_back(left.real+right.real);
                    break;
                }
                case SUB:
                {
                    valstack->push_back(left.real-right.real);
                    break;
                }
                case MUL:
                {
                    valstack->push_back(left.real*right.real);
                    break;
                }
                case DIV:
                {
                    valstack->push_back(left.real/right.real);
                    break;
                }
                case EQ:
                {
                    valstack->push_back(left.real==right.real);
                    break;
                }
                case NEQ:
                {
                    valstack->push_back(left.real!=right.real);
                    break;
                }
                case GTE:
                {
                    valstack->push_back(left.real>=right.real);
                    break;
                }
                case LTE:
                {
                    valstack->push_back(left.real<=right.real);
                    break;
                }
                case GT:
                {
                    valstack->push_back(left.real>right.real);
                    break;
                }
                case LT:
                {
                    valstack->push_back(left.real<right.real);
                    break;
                }
                case AND:
                {
                    valstack->push_back(left.real&&right.real);
                    break;
                }
                case OR:
                {
                    valstack->push_back(left.real||right.real);
                    break;
                }
                default:
                printf("Unknown binary numeric operation 0x%02X at 0x%08X\n", bytecode[pc-1], pc-1);
                return;
                }
            }
            else if(!right.is_number and !left.is_number)
            {
                switch(bytecode[pc++])
                {
                case ADD:
                {
                    valstack->push_back(left.text+right.text);
                    break;
                }
                case EQ:
                {
                    valstack->push_back(left.text==right.text);
                    break;
                }
                case NEQ:
                {
                    valstack->push_back(left.text!=right.text);
                    break;
                }
                default:
                printf("Unknown binary string operation 0x%02X at 0x%08X\n", bytecode[pc-1], pc-1);
                return;
                }
            }
            else
            {
                printf("Error: tried to apply a binary operation to a string and a number at 0x%08X\n", pc-1);
                return;
            }
            break;
        }
        case UNOP:
        {
            if(valstack->size() < 1)
            {
                puts("Error: not enough arguments to unary operation");
                return;
            }
            value right = valstack->back();
            valstack->pop_back();
            
            // TODO: Make negative operator reverse strings, maybe?
            if(right.is_number)
            {
                switch(bytecode[pc++])
                {
                case POSITIVE:
                {
                    valstack->push_back(right);
                    break;
                }
                case NEGATIVE:
                {
                    valstack->push_back(-right.real);
                    break;
                }
                case NEGATION:
                {
                    valstack->push_back(!right.real);
                    break;
                }
                default:
                printf("Unknown unary numeric operation 0x%02X at 0x%08X\n", bytecode[pc-1], pc-1);
                return;
                }
            }
            else
            {
                printf("Error: tried to apply a unary operation to a string at 0x%08X\n", pc-1);
                return;
            }
            
            break;
        }
        case BINAS:
        {
            if(valstack->size() < 1)
            {
                puts("Error: not enough arguments to binary assignment");
                return;
            }
            auto right = valstack->back();
            valstack->pop_back();
            
            std::string name;
            uint8_t c = bytecode[pc++];
            while (c != 0)
            {
                name += c;
                c = bytecode[pc++];
            }
            
            if(variables.size() == 0)
            {
                puts("Internal error: no stack of variables");
                exit(0);
            }
            auto varstack_current = variables.size()-1;
            while(variables[varstack_current].count(name) == 0 and varstack_current > 0)
                varstack_current--;
            auto & vstack = variables[varstack_current];
            if(vstack.count(name))
            {
                if(vstack[name].is_number and right.is_number)
                {
                    switch(bytecode[pc++])
                    {
                    case ASSIGN:
                    {
                        vstack[name] = right;
                        break;
                    }
                    case MUTADD:
                    {
                        vstack[name].real += right.real;
                        break;
                    }
                    case MUTSUB:
                    {
                        vstack[name].real -= right.real;
                        break;
                    }
                    case MUTMUL:
                    {
                        vstack[name].real *= right.real;
                        break;
                    }
                    case MUTDIV:
                    {
                        vstack[name].real /= right.real;
                        break;
                    }
                    default:
                    printf("Unknown binary numeric assignment 0x%02X at 0x%08X\n", bytecode[pc-1], pc-1);
                    return;
                    }
                }
                else if(!vstack[name].is_number and !right.is_number)
                {
                    switch(bytecode[pc++])
                    {
                    case ASSIGN:
                    {
                        vstack[name] = right;
                        break;
                    }
                    case MUTADD:
                    {
                        vstack[name].text += right.text;
                        break;
                    }
                    default:
                    printf("Unknown binary assignment 0x%02X at 0x%08X\n", bytecode[pc-1], pc-1);
                    return;
                    }
                }
            }
            else
            {
                puts("Error: tried to assign to undeclared variable");
                return;
            }
            
            break;
        }
        case UNAS:
        {
            std::string name;
            uint8_t c = bytecode[pc++];
            while (c != 0)
            {
                name += c;
                c = bytecode[pc++];
            }
            auto varstack_current = variables.size()-1;
            while(variables[varstack_current].count(name) == 0 and varstack_current > 0)
                varstack_current--;
            auto & vstack = variables[varstack_current];
            if(vstack.count(name))
            {
                if(vstack[name].is_number)
                {
                    switch(bytecode[pc++])
                    {
                    case INCREMENT:
                    {
                        vstack[name].real += 1;
                        break;
                    }
                    case DECREMENT:
                    {
                        vstack[name].real -= 1;
                        break;
                    }
                    default:
                    printf("Unknown unary numeric assignment 0x%02X at 0x%08X\n", bytecode[pc-1], pc-1);
                    return;
                    }
                }
                else
                {
                    printf("Tried to apply unary numeric assignment to string at 0x%08X\n", pc-1);
                    return;
                }
            }
            else
            {
                puts("Error: tried to assign to undeclared variable");
                return;
            }
            
            break;
        }
        case TRUTH:
        {
            if(valstack->size() < 1)
            {
                puts("Error: not enough arguments to set truth register");
                return;
            }
            value truth = valstack->back();
            valstack->pop_back();
            
            if(truth.is_number)
            {
                truth_register = !!truth.real;
            }
            else
            {
                puts("Error: tried to take truth of string");
                return;
            }
            
            break;
        }
        case OPENSCOPE:
        {
            variables.push_back({});
            stack.push_back({});
            
            varstack = &variables.back();
            valstack = &stack.back();
            
            break;
        }
        case EXITSCOPE:
        {
            variables.pop_back();
            stack.pop_back();
            
            varstack = &variables.back();
            valstack = &stack.back();
            
            break;
        }
        case SAVESCOPE:
        {
            stackdepths.push_back(variables.size());
            break;
        }
        case LOADSCOPE:
        {
            auto target_depth = stackdepths.back();
            while(variables.size() > target_depth)
            {
                variables.pop_back();
                stack.pop_back();
            }
            stackdepths.pop_back();
            
            break;
        }
        case BREAK:
        {
            auto target_depth = stackdepths.back();
            while(variables.size() > target_depth)
            {
                variables.pop_back();
                stack.pop_back();
            }
            stackdepths.pop_back();
            
            uint8_t b1 = bytecode[pc++];
            uint8_t b2 = bytecode[pc++];
            uint8_t b3 = bytecode[pc++];
            uint8_t b4 = bytecode[pc++];
            uint8_t b5 = bytecode[pc++];
            uint8_t b6 = bytecode[pc++];
            uint8_t b7 = bytecode[pc++];
            uint8_t b8 = bytecode[pc++];
            // big endian
            uint64_t temp = 0;
            temp |= uint64_t(b1)<<(8*7);
            temp |= uint64_t(b2)<<(8*6);
            temp |= uint64_t(b3)<<(8*5);
            temp |= uint64_t(b4)<<(8*4);
            temp |= uint64_t(b5)<<(8*3);
            temp |= uint64_t(b6)<<(8*2);
            temp |= uint64_t(b7)<<(8*1);
            temp |= uint64_t(b8)<<(8*0);
            
            int64_t offset = static_cast<int64_t>(temp);
            
            pc = loc+offset;
            
            break;
        }
        case JSIT:
        case JSIF:
        case JS:
        {
            uint8_t b1 = bytecode[pc++];
            uint8_t b2 = bytecode[pc++];
            // big endian
            uint16_t temp = 0;
            temp |= uint16_t(b1)<<(8*1);
            temp |= uint16_t(b2)<<(8*0);
            
            int16_t offset;
            if(temp >= 0x8000)
                offset = int32_t(temp)-0x10000;
            else
                offset = temp;
            
            if(opcode == JSIT)
            {
                if(truth_register)
                    pc = loc+offset;
            }
            else if(opcode == JSIF)
            {
                if(!truth_register)
                    pc = loc+offset;
            }
            else
            {
                pc = loc+offset;
            }
            
            break;
        }
        case JLIT:
        case JLIF:
        case JL:
        {
            uint8_t b1 = bytecode[pc++];
            uint8_t b2 = bytecode[pc++];
            uint8_t b3 = bytecode[pc++];
            uint8_t b4 = bytecode[pc++];
            uint8_t b5 = bytecode[pc++];
            uint8_t b6 = bytecode[pc++];
            uint8_t b7 = bytecode[pc++];
            uint8_t b8 = bytecode[pc++];
            // big endian
            uint64_t temp = 0;
            temp |= uint64_t(b1)<<(8*7);
            temp |= uint64_t(b2)<<(8*6);
            temp |= uint64_t(b3)<<(8*5);
            temp |= uint64_t(b4)<<(8*4);
            temp |= uint64_t(b5)<<(8*3);
            temp |= uint64_t(b6)<<(8*2);
            temp |= uint64_t(b7)<<(8*1);
            temp |= uint64_t(b8)<<(8*0);
            
            int64_t offset = static_cast<int64_t>(temp);
            
            if(opcode == JLIT)
            {
                if(truth_register)
                    pc = loc+offset;
            }
            else if(opcode == JLIF)
            {
                if(!truth_register)
                    pc = loc+offset;
            }
            else
            {
                pc = loc+offset;
            }
            
            break;
        }
        case CALL:
        {
            std::string name;
            uint8_t c = bytecode[pc++];
            while (c != 0)
            {
                name += c;
                c = bytecode[pc++];
            }
            uint8_t args = bytecode[pc++];
            if(valstack->size() < args)
            {
                puts("Error: function call uses more arguments than are on stack");
                return;
            }
            std::vector<value> arguments;
            for(int i = 0; i < args; i++)
            {
                arguments.push_back(valstack->back());
                valstack->pop_back();
            }
            if(name == "print")
            {
                if(args != 1)
                {
                    puts("Error: wrong number of arguments to function \"print\"");
                    return;
                }
                if(arguments[0].is_number)
                    printf("%f\n", arguments[0].real);
                else
                    printf("%s\n", arguments[0].text.data());
            }
            else
            {
                printf("Error: unknown function \"%s\"\n", name.data());
                return;
            }
            break;
        }
        case FUNCDEF:
        {
            // TODO: implement
            puts("Unimplemented FUNCDEF");
            return;
            break;
        }
        case RETURN:
        {
            // TODO: implement
            puts("Unimplemented RETURN");
            return;
            break;
        }
        default:
        printf("Unknown instruction 0x%02X at 0x%08X\n", bytecode[pc-1], pc-1);
        exit(0);
        return;
        }
    }
}

void disassemble(progstate * program)
{
    if(program == nullptr)
    {
        puts("Program is nullptr");
        return;
    }
    auto & pc = program->pc;
    auto & bytecode = program->bytecode;
    while(1)
    {
        if(pc >= bytecode.size())
        {
            return;
        }
        printf("%08X: ", pc);
        auto opcode = bytecode[pc++];
        switch(opcode)
        {
        case NOP:
        {
            break;
        }
        case PUSHVAL:
        {
            uint8_t b1 = bytecode[pc++];
            uint8_t b2 = bytecode[pc++];
            uint8_t b3 = bytecode[pc++];
            uint8_t b4 = bytecode[pc++];
            uint8_t b5 = bytecode[pc++];
            uint8_t b6 = bytecode[pc++];
            uint8_t b7 = bytecode[pc++];
            uint8_t b8 = bytecode[pc++];
            // big endian
            uint64_t temp = 0;
            temp |= uint64_t(b1)<<(8*7);
            temp |= uint64_t(b2)<<(8*6);
            temp |= uint64_t(b3)<<(8*5);
            temp |= uint64_t(b4)<<(8*4);
            temp |= uint64_t(b5)<<(8*3);
            temp |= uint64_t(b6)<<(8*2);
            temp |= uint64_t(b7)<<(8*1);
            temp |= uint64_t(b8)<<(8*0);
            double value;
            memcpy(&value, &temp, sizeof(double));
            
            printf("PUSHVAL %f\n", value);
            break;
        }
        case PUSHTEXT:
        {
            std::string text;
            uint8_t c = bytecode[pc++];
            while (c != 0)
            {
                text += c;
                c = bytecode[pc++];
            }
            
            printf("PUSHTEXT \"%s\"\n", text.data());
            break;
        }
        case PUSHVAR:
        {
            std::string name;
            uint8_t c = bytecode[pc++];
            while (c != 0)
            {
                name += c;
                c = bytecode[pc++];
            }
            
            printf("PUSHVAR %s\n", name.data());
            
            break;
        }
        case DECLARE:
        {
            std::string name;
            uint8_t c = bytecode[pc++];
            while (c != 0)
            {
                name += c;
                c = bytecode[pc++];
            }
            
            printf("DECLARE %s\n", name.data());
            
            break;
        }
        case DECLSET:
        {
            std::string name;
            uint8_t c = bytecode[pc++];
            while (c != 0)
            {
                name += c;
                c = bytecode[pc++];
            }
            
            printf("DECLSET %s\n", name.data());
            
            break;
        }
        case BINOP:
        {
            switch(bytecode[pc++])
            {
            case ADD:
            {
                puts("BINOP ADD");
                break;
            }
            case SUB:
            {
                puts("BINOP SUB");
                break;
            }
            case MUL:
            {
                puts("BINOP MUL");
                break;
            }
            case DIV:
            {
                puts("BINOP DIV");
                break;
            }
            case EQ:
            {
                puts("BINOP EQ");
                break;
            }
            case NEQ:
            {
                puts("BINOP NEQ");
                break;
            }
            case GTE:
            {
                puts("BINOP GTE");
                break;
            }
            case LTE:
            {
                puts("BINOP LTE");
                break;
            }
            case GT:
            {
                puts("BINOP GT");
                break;
            }
            case LT:
            {
                puts("BINOP LT");
                break;
            }
            case AND:
            {
                puts("BINOP AND");
                break;
            }
            case OR:
            {
                puts("BINOP OR");
                break;
            }
            default:
            printf("Unknown unary numeric operation 0x%02X at 0x%08X\n", bytecode[pc-1], pc-1);
            return;
            }
            break;
        }
        case UNOP:
        {
            switch(bytecode[pc++])
            {
            case POSITIVE:
            {
                puts("UNOP POSITIVE");
                break;
            }
            case NEGATIVE:
            {
                puts("UNOP NEGATIVE");
                break;
            }
            case NEGATION:
            {
                puts("UNOP NEGATION");
                break;
            }
            default:
            printf("Unknown unary numeric operation 0x%02X at 0x%08X\n", bytecode[pc-1], pc-1);
            return;
            }
            
            break;
        }
        case BINAS:
        {
            std::string name;
            uint8_t c = bytecode[pc++];
            while (c != 0)
            {
                name += c;
                c = bytecode[pc++];
            }
            
            switch(bytecode[pc++])
            {
            case ASSIGN:
            {
                printf("BINAS %s ASSIGN\n", name.data());
                break;
            }
            case MUTADD:
            {
                printf("BINAS %s MUTADD\n", name.data());
                break;
            }
            case MUTSUB:
            {
                printf("BINAS %s MUTSUB\n", name.data());
                break;
            }
            case MUTMUL:
            {
                printf("BINAS %s MUTMUL\n", name.data());
                break;
            }
            case MUTDIV:
            {
                printf("BINAS %s MUTDIV\n", name.data());
                break;
            }
            default:
            printf("Unknown binary numeric assignment 0x%02X at 0x%08X\n", bytecode[pc-1], pc-1);
            return;
            }
            
            
            break;
        }
        case UNAS:
        {
            std::string name;
            uint8_t c = bytecode[pc++];
            while (c != 0)
            {
                name += c;
                c = bytecode[pc++];
            }
            
            switch(bytecode[pc++])
            {
            case INCREMENT:
            {
                printf("UNAS %s INCREMENT\n", name.data());
                break;
            }
            case DECREMENT:
            {
                printf("UNAS %s DECREMENT\n", name.data());
                break;
            }
            default:
            printf("Unknown unary numeric assignment 0x%02X at 0x%08X\n", bytecode[pc-1], pc-1);
            return;
            }
            
            break;
        }
        case TRUTH:
        {
            puts("TRUTH");
            break;
        }
        case OPENSCOPE:
        {
            puts("OPENSCOPE");
            break;
        }
        case EXITSCOPE:
        {
            puts("EXITSCOPE");
            break;
        }
        case SAVESCOPE:
        {
            puts("LOADSCOPE");
            break;
        }
        case LOADSCOPE:
        {
            puts("SAVESCOPE");
            break;
        }
        case BREAK:
        {
            uint8_t b1 = bytecode[pc++];
            uint8_t b2 = bytecode[pc++];
            uint8_t b3 = bytecode[pc++];
            uint8_t b4 = bytecode[pc++];
            uint8_t b5 = bytecode[pc++];
            uint8_t b6 = bytecode[pc++];
            uint8_t b7 = bytecode[pc++];
            uint8_t b8 = bytecode[pc++];
            // big endian
            uint64_t temp = 0;
            temp |= uint64_t(b1)<<(8*7);
            temp |= uint64_t(b2)<<(8*6);
            temp |= uint64_t(b3)<<(8*5);
            temp |= uint64_t(b4)<<(8*4);
            temp |= uint64_t(b5)<<(8*3);
            temp |= uint64_t(b6)<<(8*2);
            temp |= uint64_t(b7)<<(8*1);
            temp |= uint64_t(b8)<<(8*0);
            
            int64_t offset = static_cast<int64_t>(temp);
            
            printf("BREAK %d\n", offset);
            
            break;
        }
        case JSIT:
        case JSIF:
        case JS:
        {
            uint8_t b1 = bytecode[pc++];
            uint8_t b2 = bytecode[pc++];
            // big endian
            uint16_t temp = 0;
            temp |= uint16_t(b1)<<(8*1);
            temp |= uint16_t(b2)<<(8*0);
            
            int16_t offset;
            if(temp >= 0x8000)
                offset = int32_t(temp)-0x10000;
            else
                offset = temp;
            
            if(opcode == JSIT)
                printf("JSIT");
            if(opcode == JSIF)
                printf("JSIF");
            if(opcode == JS)
                printf("JS");
            printf(" %d\n", offset);
            
            break;
        }
        case JLIT:
        case JLIF:
        case JL:
        {
            uint8_t b1 = bytecode[pc++];
            uint8_t b2 = bytecode[pc++];
            uint8_t b3 = bytecode[pc++];
            uint8_t b4 = bytecode[pc++];
            uint8_t b5 = bytecode[pc++];
            uint8_t b6 = bytecode[pc++];
            uint8_t b7 = bytecode[pc++];
            uint8_t b8 = bytecode[pc++];
            // big endian
            uint64_t temp = 0;
            temp |= uint64_t(b1)<<(8*7);
            temp |= uint64_t(b2)<<(8*6);
            temp |= uint64_t(b3)<<(8*5);
            temp |= uint64_t(b4)<<(8*4);
            temp |= uint64_t(b5)<<(8*3);
            temp |= uint64_t(b6)<<(8*2);
            temp |= uint64_t(b7)<<(8*1);
            temp |= uint64_t(b8)<<(8*0);
            
            int64_t offset = static_cast<int64_t>(temp);
            
            if(opcode == JLIT)
                printf("JLIT");
            if(opcode == JLIF)
                printf("JLIF");
            if(opcode == JL)
                printf("JL");
            printf(" %d\n", offset);
            
            break;
        }
        case CALL:
        {
            std::string name;
            uint8_t c = bytecode[pc++];
            while (c != 0)
            {
                name += c;
                c = bytecode[pc++];
            }
            uint8_t args = bytecode[pc++];
            
            printf("CALL %s %d\n", name.data(), args);
            
            break;
        }
        case FUNCDEF:
        {
            puts("FUNCDEF");
            break;
        }
        case RETURN:
        {
            puts("RETURN");
            break;
        }
        default:
        printf("Unknown instruction 0x%02X at 0x%08X\n", bytecode[pc-1], pc-1);
        exit(0);
        return;
        }
    }
}

/*
int main()
{
    progstate myprogram;
    myprogram.bytecode = {
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x40, 0x09, 0x1e, 0xb8, 0x51, 0xeb, 0x85, 0x1f,
        0x16, 0x70, 0x72, 0x69, 0x6E, 0x74, 0x00, 0x01,
        0x12, 0xFF, 0xEF, 
    };
    interpret(&myprogram);
    return 0;
}
*/

/*
TODO list
- implement for(;;){}
- implement funcdef
- implement objects and with(){}
- implement more standard functions
*/
