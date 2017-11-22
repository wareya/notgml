#include <stdio.h>
#include <stdlib.h>

#include <vector>
#include <map>
#include <string>

template <typename T>
void vector_append(std::vector<T> * a, const std::vector<T> & b)
{
    a->reserve(a->size() + b.size());
    std::copy(std::begin(b), std::end(b), std::back_inserter(*a));
    //a->insert(std::end(*a), std::begin(b), std::end(b));
}

std::vector<std::string> ops;

struct token
{
    uint64_t position = 0;
    uint64_t endposition = 0;
    std::string text;
};

bool verbose = false;

bool is_whitespace(char c)
{
    return (c == ' ' or c == '\t' or c == '\n' or c == '\r');
}

bool is_number(char c)
{
    return (c >= '0' and c <= '9');
}

bool is_alpha(char c)
{
    return ((c >= 'A' and c <= 'Z') or (c >= 'a' and c <= 'z'));
}

bool is_alphanum(char c)
{
    return is_number(c) or is_alpha(c);
}

bool is_namestart(char c)
{
    return is_alpha(c) or c == '_';
}

bool is_namecont(char c)
{
    return is_alphanum(c) or c == '_';
}

bool is_number(std::string s)
{
    if(s.size() == 0) return false;
    
    for(char c : s)
        if(!is_number(c))
            return false;
    return true;
}

bool is_string(std::string s)
{
    if(s.size() < 2) return false;
    
    if(s[0] != '"' or s[s.size()-1] != '"')
        return false;
    
    return true;
}

bool is_name(std::string s)
{
    if(s.size() == 0) return false;
    
    bool first = true;
    for(char c : s)
    {
        if(first)
        {
            if(!is_namestart(c))
                return false;
        }
        else
        {
            if(!is_namecont(c))
                return false;
        }
    }
    return true;
}

bool is_order(std::string s)
{
    return (s == "break" or s == "continue");
}

std::vector<token> lex(std::string str)
{
    std::vector<token> tokenlist;
    
    uint64_t i = 0;
    while(i < str.size())
    {
        if(is_whitespace(str[i]))
        {
            i++;
        }
        else
        {
            bool matched = false;
            for(const auto & op : ops)
            {
                uint64_t j;
                for(j = 0; j < op.size(); j++)
                {
                    if(str[i+j] != op[j])
                        break;
                }
                if(j == op.size())// and (i+j == str.size() or !is_namestart(op[0]) or (is_namestart(op[0]) and !is_namestart(str[i+j]))))
                {
                    tokenlist.push_back({i, i+j, op});
                    i += j;
                    matched = true;
                    break;
                }
            }
            if(matched)
                continue;
            else if(is_number(str[i]))
            {
                uint64_t j;
                for(j = i+1; j < str.size() and is_number(str[j]); j++);
                tokenlist.push_back({i, j, str.substr(i, j-i)});
                i = j;
            }
            else if(is_namestart(str[i]))
            {
                uint64_t j;
                for(j = i+1; j < str.size() and is_namecont(str[j]); j++);
                tokenlist.push_back({i, j, str.substr(i, j-i)});
                i = j;
            }
            else if(str[i] == '"')
            {
                std::string text;
                bool escaping = false;
                for(uint64_t j = i; j < str.size(); j++)
                {
                    auto c = str[j];
                    if(c == '\\' and !escaping)
                    {
                        escaping = true;
                    }
                    else if(c == '\\' and escaping)
                    {
                        escaping = false;
                        text += c;
                    }
                    else if(c == 'n' and escaping)
                    {
                        escaping = false;
                        text += '\n';
                    }
                    else if(c == '"' and escaping)
                    {
                        escaping = false;
                        text += '"';
                    }
                    else if(escaping)
                    {
                        printf("Error lexing: unknown escape sequence \\%c\n", c);
                        return {};
                    }
                    else if(c == '"' and j > i)
                    {
                        text += c;
                        break;
                    }
                    else
                    {
                        text += c;
                    }
                }
                tokenlist.push_back({i, i+text.length(), text});
                i += text.length();
            }
            else
            {
                puts("Error lexing: unrecognized token");
                puts(str.data());
                for(uint64_t j = 0; j < i; j++)
                    printf(" ");
                puts("^");
                printf("Tokens so far: ");
                for(const auto & t : tokenlist)
                    printf("%s ", t.text.data());
                puts("");
                return {};
            }
        }
    }
    return tokenlist;
}

struct node {
    bool iserror = false;
    bool iseos = false;
    std::string identity;
    std::string text;
    std::string error;
    int position;
    int errorpos = 0;
    node * parent = nullptr;
    node * left = nullptr;
    node * right = nullptr;
    int arraynodes = 0;
    node ** nodearray = nullptr;
};

void delete_node(node * mynode)
{
    if(mynode == nullptr) return;
    delete_node(mynode->left);
    delete_node(mynode->right);
    if(mynode->arraynodes)
    {
        for(int i = 0; i < mynode->arraynodes; i++)
            delete_node(mynode->nodearray[i]);
        free(mynode->nodearray);
    }
    delete mynode;
}

void delete_tree(node * mynode)
{
    while(mynode->parent != nullptr)
        mynode = mynode->parent;
    delete_node(mynode);
    /*
    if(mynode == nullptr) return;
    delete_node(mynode->left);
    delete_node(mynode->right);
    mynode->left = nullptr;
    mynode->right = nullptr;
    if(mynode->parent != nullptr)
        delete_tree(mynode->parent);
    else
        delete mynode;
    */
}

node * unexpected_eos(int position)
{
    auto mynode = new node;
    mynode->iserror = true;
    mynode->iseos = true;
    mynode->error = "Error: unexpected end of stream";
    mynode->identity = "eos";
    mynode->text = "";
    mynode->position = position;
    //if(verbose) puts("Unexpected EOS");
    return mynode;
}

void print_node(node * mynode, bool root = true)
{
    if(mynode == nullptr) return;
    while(root and mynode->parent)
        mynode = mynode->parent;
    
    if(mynode->identity == "()")
        print_node(mynode->right, false);
    else
    {
        if(mynode->text == "")
            printf("%s", mynode->identity.data());
        else
            printf("%s<%s>", mynode->identity.data(), mynode->text.data());
        if(mynode->left or mynode->right)
            printf("(");
        if(mynode->left)
            print_node(mynode->left, false);
        if(mynode->left or mynode->right)
            printf(",");
        if(mynode->right)
            print_node(mynode->right, false);
        if(mynode->left or mynode->right)
            printf(")");
        if(mynode->arraynodes)
        {
            printf("[");
            for(int i = 0; i < mynode->arraynodes; i++)
            {
                print_node(mynode->nodearray[i], false);
                if(i+1 != mynode->arraynodes)
                    printf(",");
            }
            printf("]");
        }
    }
}

node * parse_expression(const std::vector<token> & tokens, int i, int & consumed);

node * parse_exp_paren(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    if(tokens[i].text == "(")
    {
        auto mynode = new node;
        mynode->identity = "exp_paren";
        mynode->text = "()";
        mynode->position = i;
        
        i++;
        int consume = 0;
        auto rhs = parse_expression(tokens, i, consume);
        i += consume;
        mynode->right = rhs;
        rhs->parent = mynode;
        if(rhs->iserror or i >= tokens.size() or tokens[i].text != ")")
        {
            mynode->iserror = true;
            mynode->error = "Error: expected closing paren";
            if(verbose) puts("closing paren error");
            if(verbose) puts(rhs->error.data());
            //printf("%d %d %d %d %d %s\n", rhs->iserror, rhs->iseos, i, consume, tokens.size(), tokens[i].text.data());
            mynode->errorpos = tokens[i].position;
            return mynode;
        }
        
        consumed = consume+2;
        return mynode;
    }
    else
    {
        consumed = 0;
        auto mynode = new node;
        mynode->identity = "exp_paren";
        mynode->iserror = true;
        mynode->error = "Error: expected ( at start of parenthetical expression";
        mynode->position = i;
        mynode->errorpos = tokens[i].position;
        if(verbose) puts("Paren expression signalling error");
        return mynode;
    }

}

node * parse_exp_corevalue(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    if(tokens[i].text == "(")
    {
        return parse_exp_paren(tokens, i, consumed);
    }
    else if(is_number(tokens[i].text))
    {
        consumed = 1;
        auto mynode = new node;
        mynode->identity = "corevalue";
        mynode->text = tokens[i].text;
        mynode->position = i;
        return mynode;
    }
    else if(is_name(tokens[i].text))
    {
        consumed = 1;
        auto mynode = new node;
        mynode->identity = "name";
        mynode->text = tokens[i].text;
        mynode->position = i;
        return mynode;
    }
    else if(is_string(tokens[i].text))
    {
        consumed = 1;
        auto mynode = new node;
        mynode->identity = "corevalue";
        mynode->text = tokens[i].text;
        mynode->position = i;
        return mynode;
    }
    else
    {
        consumed = 0;
        auto mynode = new node;
        mynode->identity = "corevalue";
        mynode->iserror = true;
        mynode->error = "Error: not a value";
        mynode->position = i;
        mynode->errorpos = tokens[i].position;
        if(verbose) puts("Value error");
        return mynode;
    }
}

node * parse_exp_value(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    if(tokens[i].text == "-" or tokens[i].text == "+" or tokens[i].text == "!")
    {
        auto mynode = new node;
        mynode->identity = "unary_op";
        mynode->text = tokens[i].text;
        mynode->position = i;
        mynode->right = parse_exp_value(tokens, i+1, consumed);
        consumed++;
        return mynode;
    }
    else
        return parse_exp_corevalue(tokens, i, consumed);
}

typedef node*(*parser)(const std::vector<token> & tokens, int i, int & consumed);

node * binary_parse_tail(const std::vector<token> & tokens, int i, int & consumed, const std::vector<std::string> opsymbols, parser d1)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    for(auto op : opsymbols)
    {
        if(tokens[i].text == op)
        {
            auto mynode = new node;
            mynode->identity = "binary_op";
            mynode->text = op;
            mynode->position = i;
            i++;
            
            int consume;
            auto rhs = d1(tokens, i, consume);
            mynode->right = rhs;
            rhs->parent = mynode;
            if(rhs->iserror)
            {
                consumed = 0;
                mynode->iserror = true;
                mynode->error = "Error: expected expression";
                if(verbose) puts("exp expr error");
                if(verbose) puts(op.data());
                if(i < tokens.size())
                {
                    if(verbose) printf("%d %d\n", i, (int)tokens[i].position);
                }
                else
                {
                    if(verbose) printf("%d %d\n", i, (int)tokens.size()+1);
                }
                mynode->errorpos = rhs->errorpos;
                return mynode;
            }
            else
            {
                consumed = consume+1;
                return mynode;
            }
        }
    }
    
    consumed = 0;
    auto mynode = new node;
    mynode->identity = "binary_op";
    mynode->iserror = true;
    mynode->error = "Error: unknown symbol '" + tokens[i].text + "'";
    mynode->position = i;
    mynode->errorpos = tokens[i].position;
    if(verbose) puts("Unknown symbol error");
    return mynode;
}

node * binary_parse(const std::vector<token> & tokens, int i, int & consumed, parser d1_tail, parser d2)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    int consumed_left, consumed_right;
    auto left = d2(tokens, i, consumed_left);
    if(consumed_left > 0)
    {
        auto right = d1_tail(tokens, i+consumed_left, consumed_right);
        if(!right->iserror and !left->iserror)
        {
            right->left = left;
            left->parent = right;
            consumed = consumed_left + consumed_right;
            return right;
        }
        else
        {
            delete_node(right);
            consumed = consumed_left;
            return left;
        }
    }
    else
    {
        if(verbose) puts("bailing out left side");
        consumed = consumed_left;
        return left;
    }
}

node * parse_exp_d5(const std::vector<token> & tokens, int i, int & consumed)
{
    return parse_exp_value(tokens, i, consumed);
}

node * parse_exp_d4(const std::vector<token> & tokens, int i, int & consumed);
node * parse_exp_d4_tail(const std::vector<token> & tokens, int i, int & consumed)
{
    return binary_parse_tail(tokens, i, consumed, {"*", "/"}, parse_exp_d4);
}
node * parse_exp_d4(const std::vector<token> & tokens, int i, int & consumed)
{
    return binary_parse(tokens, i, consumed, parse_exp_d4_tail, parse_exp_d5);
}

node * parse_exp_d3(const std::vector<token> & tokens, int i, int & consumed);
node * parse_exp_d3_tail(const std::vector<token> & tokens, int i, int & consumed)
{
    return binary_parse_tail(tokens, i, consumed, {"+", "-"}, parse_exp_d3);
}
node * parse_exp_d3(const std::vector<token> & tokens, int i, int & consumed)
{
    return binary_parse(tokens, i, consumed, parse_exp_d3_tail, parse_exp_d4);
}

node * parse_exp_d2(const std::vector<token> & tokens, int i, int & consumed);
node * parse_exp_d2_tail(const std::vector<token> & tokens, int i, int & consumed)
{
    return binary_parse_tail(tokens, i, consumed, {"==", "!=", ">=", "<=", ">", "<"}, parse_exp_d2);
}
node * parse_exp_d2(const std::vector<token> & tokens, int i, int & consumed)
{
    return binary_parse(tokens, i, consumed, parse_exp_d2_tail, parse_exp_d3);
}

node * parse_exp_d1(const std::vector<token> & tokens, int i, int & consumed);
node * parse_exp_d1_tail(const std::vector<token> & tokens, int i, int & consumed)
{
    return binary_parse_tail(tokens, i, consumed, {"&&", "||", "and", "or"}, parse_exp_d1);
}
node * parse_exp_d1(const std::vector<token> & tokens, int i, int & consumed)
{
    return binary_parse(tokens, i, consumed, parse_exp_d1_tail, parse_exp_d2);
}

node * parse_expression(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    return parse_exp_d1(tokens, i, consumed);
}

node * parse_block(const std::vector<token> & tokens, int i, int & consumed);
node * parse_condition_else(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    if(tokens[i].text == "else")
    {
        auto mynode = new node;
        mynode->identity = "condition_else";
        mynode->text = "else";
        mynode->position = i;
        consumed = 1;
        
        int consumed_statement = 0;
        
        auto statement = parse_block(tokens, i+1, consumed_statement);
        
        if(statement->iserror)
        {
            mynode->iserror = true;
            mynode->error = statement->error;
            mynode->errorpos = statement->errorpos;
            delete_node(statement);
            consumed = 0;
            return mynode;
        }
        
        mynode->right = statement;
        statement->parent = mynode;
        consumed = 1+consumed_statement;
        return mynode;
    }
    else
    {
        consumed = 0;
        auto mynode = new node;
        mynode->iserror = true;
        mynode->error = "Error: expected else at start of \"else\" condition";
        mynode->position = i;
        mynode->errorpos = tokens[i].position;
        if(verbose) puts("Else condition signalling error");
        return mynode;
    }
}
node * parse_condition_if(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    if(tokens[i].text == "if")
    {
        auto mynode = new node;
        mynode->identity = "condition_if";
        mynode->text = "if";
        mynode->position = i;
        consumed = 1;
        
        int consumed_expr, consumed_statement, consumed_else;
        consumed_expr = 0;
        consumed_statement = 0;
        consumed_else = 0;
        
        auto expr = parse_exp_paren(tokens, i+1, consumed_expr);
        
        if(expr->iserror)
        {
            mynode->iserror = true;
            mynode->error = expr->error;
            mynode->errorpos = expr->errorpos;
            delete_node(expr);
            consumed = 0;
            return mynode;
        }
        
        auto statement = parse_block(tokens, i+1+consumed_expr, consumed_statement);
        
        if(statement->iserror)
        {
            mynode->iserror = true;
            mynode->error = statement->error;
            mynode->errorpos = statement->errorpos;
            delete_node(expr);
            delete_node(statement);
            consumed = 0;
            return mynode;
        }
        
        auto myelse = parse_condition_else(tokens, i+1+consumed_expr+consumed_statement, consumed_else);
        
        if(myelse->iserror)
        {
            delete_node(myelse);
            
            mynode->arraynodes = 2;
            mynode->nodearray = (node **)malloc(sizeof(node*)*mynode->arraynodes);
            mynode->nodearray[0] = expr;
            mynode->nodearray[1] = statement;
            expr->parent = mynode;
            statement->parent = mynode;
            
            consumed = 1+consumed_expr+consumed_statement;
            return mynode;
        }
        else
        {
            mynode->arraynodes = 3;
            mynode->nodearray = (node **)malloc(sizeof(node*)*mynode->arraynodes);
            mynode->nodearray[0] = expr;
            mynode->nodearray[1] = statement;
            mynode->nodearray[2] = myelse;
            expr->parent = mynode;
            statement->parent = mynode;
            myelse->parent = mynode;
            
            consumed = 1+consumed_expr+consumed_statement+consumed_else;
            return mynode;
        }
    }
    else
    {
        consumed = 0;
        auto mynode = new node;
        mynode->iserror = true;
        mynode->error = "Error: expected if at start of \"if\" condition";
        mynode->position = i;
        mynode->errorpos = tokens[i].position;
        if(verbose) puts("If condition signalling error");
        return mynode;
    }
}
node * parse_condition_while(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    if(tokens[i].text == "while")
    {
        auto mynode = new node;
        mynode->identity = "condition_while";
        mynode->text = "while";
        mynode->position = i;
        consumed = 1;
        
        int consumed_expr, consumed_statement;
        consumed_expr = 0;
        consumed_statement = 0;
        
        auto expr = parse_exp_paren(tokens, i+1, consumed_expr);
        
        if(expr->iserror)
        {
            mynode->iserror = true;
            mynode->error = expr->error;
            mynode->errorpos = expr->errorpos;
            delete_node(expr);
            consumed = 0;
            puts("expr error in while");
            return mynode;
        }
        
        auto statement = parse_block(tokens, i+1+consumed_expr, consumed_statement);
        
        if(statement->iserror)
        {
            mynode->iserror = true;
            mynode->error = statement->error;
            mynode->errorpos = statement->errorpos;
            delete_node(expr);
            delete_node(statement);
            consumed = 0;
            puts("statement error in while");
            return mynode;
        }
        
        mynode->arraynodes = 2;
        mynode->nodearray = (node **)malloc(sizeof(node*)*mynode->arraynodes);
        mynode->nodearray[0] = expr;
        mynode->nodearray[1] = statement;
        expr->parent = mynode;
        statement->parent = mynode;
        
        consumed = 1+consumed_expr+consumed_statement;
        return mynode;
    }
    else
    {
        consumed = 0;
        auto mynode = new node;
        mynode->identity = "condition_while";
        mynode->iserror = true;
        mynode->error = "Error: expected while at start of \"while\" condition";
        mynode->position = i;
        mynode->errorpos = tokens[i].position;
        if(verbose) puts("While condition signalling error");
        return mynode;
    }
}

node * parse_declaration(const std::vector<token> & tokens, int i, int & consumed);
node * parse_mutation(const std::vector<token> & tokens, int i, int & consumed);
node * parse_instruction_bare(const std::vector<token> & tokens, int i, int & consumed);
node * parse_bigblock(const std::vector<token> & tokens, int i, int & consumed);

node * parse_condition_for_header(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    if(tokens[i].text != "(")
    {
        consumed = 0;
        auto mynode = new node;
        mynode->identity = "condition_for_header";
        mynode->iserror = true;
        mynode->error = "Error: expected opening paren of \"for\" condition";
        mynode->position = i;
        mynode->errorpos = tokens[i].position;
        return mynode;
    }
    
    int left_consumed = 0;
    auto left = parse_declaration(tokens, i+1, left_consumed);
    if(left->iserror)
    {
        delete_node(left);
        left_consumed = 0;
        
        auto left = parse_mutation(tokens, i+1, left_consumed);
        if(left->iserror)
        {
            delete_node(left);
            
            consumed = 0;
            auto mynode = new node;
            mynode->identity = "condition_for_header";
            mynode->iserror = true;
            mynode->error = "Error: expected declaration or assignment as first part of \"for\" condition";
            mynode->position = i+1;
            mynode->errorpos = tokens[i+1].position;
            return mynode;
        }
    }
    int middle_consumed = 0;
    auto middle = parse_expression(tokens, i+1+left_consumed, middle_consumed);
    if(middle->iserror)
    {
        delete_node(left);
        delete_node(middle);
        
        consumed = 0;
        auto mynode = new node;
        mynode->identity = "condition_for_header";
        mynode->iserror = true;
        mynode->error = "Error: expected expression as second part of \"for\" condition";
        mynode->position = i+1+left_consumed;
        mynode->errorpos = tokens[i+1+left_consumed].position;
        return mynode;
    }
    if (i+1+left_consumed+middle_consumed >= tokens.size() or tokens[i+1+left_consumed+middle_consumed].text != ";")
    {
        delete_node(middle);
        delete_node(left);
        
        consumed = 0;
        auto mynode = new node;
        mynode->identity = "condition_for_header";
        mynode->iserror = true;
        mynode->error = "Error: expected semicolon delimiting expression from instruction in \"for\" condition";
        mynode->position = i+1+left_consumed+middle_consumed;
        mynode->errorpos = tokens[i+1+left_consumed+middle_consumed-1].endposition;
        return mynode;
    }
    int right_consumed = 0;
    auto right = parse_instruction_bare(tokens, i+1+left_consumed+middle_consumed+1, right_consumed);
    if(right->iserror)
    {
        delete_node(right);
        right_consumed = 0;
        right = parse_bigblock(tokens, i+1+left_consumed+middle_consumed+1, right_consumed);
        
        if(right->iserror)
        {
            // TODO: fall back to bigblock
            delete_node(middle);
            delete_node(left);
            delete_node(right);
            
            consumed = 0;
            auto mynode = new node;
            mynode->identity = "condition_for_header";
            mynode->iserror = true;
            mynode->error = "Error: expected instruction as third part of \"for\" condition";
            mynode->position = i+1+left_consumed+middle_consumed+1;
            mynode->errorpos = tokens[i+1+left_consumed+middle_consumed].endposition;
            return mynode;
        }
    }
    int tentative_consumed = 1+left_consumed+middle_consumed+1+right_consumed;
    if (i+tentative_consumed >= tokens.size() or tokens[i+tentative_consumed].text != ")")
    {
        delete_node(middle);
        delete_node(left);
        delete_node(right);
        
        consumed = 0;
        auto mynode = new node;
        mynode->identity = "condition_for_header";
        mynode->iserror = true;
        mynode->error = "Error: expected closing paren to \"for\" condition";
        mynode->position = i+tentative_consumed;
        mynode->errorpos = tokens[i+tentative_consumed-1].endposition;
        return mynode;
    }
    consumed = tentative_consumed+1;
    
    auto array = (node**)malloc(sizeof(node*)*3);
    array[0] = left;
    array[1] = middle;
    array[2] = right;
    
    auto mynode = new node;
    mynode->identity = "condition_for_header";
    mynode->text = "()";
    mynode->position = i;
    mynode->arraynodes = 3;
    // this nodearray is only being stored temporarily and we do not need to set its nodes' parents to mynode
    mynode->nodearray = array;
    return mynode;
}

node * parse_condition_for(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    if(tokens[i].text == "for")
    {
        auto mynode = new node;
        mynode->identity = "condition_for";
        mynode->text = "for";
        mynode->position = i;
        consumed = 1;
        
        int consumed_expr, consumed_statement;
        consumed_expr = 0;
        consumed_statement = 0;
        
        auto header = parse_condition_for_header(tokens, i+1, consumed_expr);
        
        if(header->iserror)
        {
            mynode->iserror = true;
            mynode->error = header->error;
            mynode->errorpos = header->errorpos;
            delete_node(header);
            consumed = 0;
            puts("header/expression error in for");
            return mynode;
        }
        
        auto statement = parse_block(tokens, i+1+consumed_expr, consumed_statement);
        
        if(statement->iserror)
        {
            mynode->iserror = true;
            mynode->error = statement->error;
            mynode->errorpos = statement->errorpos;
            delete_node(header);
            delete_node(statement);
            consumed = 0;
            puts("statement error in for");
            return mynode;
        }
        
        mynode->arraynodes = 4;
        mynode->nodearray = (node **)malloc(sizeof(node*)*4);
        
        mynode->nodearray[0] = header->nodearray[0];
        mynode->nodearray[1] = header->nodearray[1];
        mynode->nodearray[2] = header->nodearray[2];
        header->nodearray[0] = nullptr;
        header->nodearray[1] = nullptr;
        header->nodearray[2] = nullptr;
        delete_node(header);
        
        mynode->nodearray[3] = statement;
        
        mynode->nodearray[0]->parent = mynode;
        mynode->nodearray[1]->parent = mynode;
        mynode->nodearray[2]->parent = mynode;
        mynode->nodearray[3]->parent = mynode;
        
        consumed = 1+consumed_expr+consumed_statement;
        return mynode;
    }
    else
    {
        consumed = 0;
        auto mynode = new node;
        mynode->identity = "condition_for";
        mynode->iserror = true;
        mynode->error = "Error: expected for at start of \"for\" condition";
        mynode->position = i;
        mynode->errorpos = tokens[i].position;
        if(verbose) puts("For condition signalling error");
        return mynode;
    }
}

node * parse_name(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    if(is_name(tokens[i].text))
    {
        auto mynode = new node;
        mynode->identity = "name";
        mynode->text = tokens[i].text;
        mynode->position = i;
        consumed = 1;
        return mynode;
    }
    else
    {
        auto mynode = new node;
        mynode->identity = "name";
        mynode->text = tokens[i].text;
        mynode->position = i;
        mynode->iserror = true;
        mynode->error = "Error: expected name";
        mynode->errorpos = tokens[i].position;
        consumed = 1;
        return mynode;
    }
}

node * parse_compound_name(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    int name_consumed = 0;
    auto name = parse_name(tokens, i, name_consumed);
    if(name->iserror)
    {
        consumed = 0;
        return name;
    }
    if(i+name_consumed+1 < tokens.size() and tokens[i+name_consumed].text == "=")
    {
        int expr_consumed = 0;
        auto expr = parse_expression(tokens, i+name_consumed+1, expr_consumed);
        if(expr->iserror)
        {
            delete_node(expr);
            goto fallback;
        }
        auto mynode = new node;
        mynode->identity = "compound_name";
        mynode->text = "=";
        mynode->position = i+name_consumed;
        mynode->left = name;
        mynode->right = expr;
        name->parent = mynode;
        expr->parent = mynode;
        consumed = name_consumed+expr_consumed+1;
        return mynode;
    }
    fallback:
    {
        delete_node(name);
        consumed = 0;
        auto mynode = new node;
        mynode->identity = "compound_name";
        mynode->iserror = true;
        mynode->error = "Error: expected compound name declaration (with an =)";
        mynode->position = i;
        mynode->errorpos = tokens[i].position;
        return mynode;
    }
}

node * parse_deflist(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    int left_consumed = 0;
    auto left = parse_compound_name(tokens, i, left_consumed);
    if(left->iserror)
    {
        delete_node(left);
        left = parse_name(tokens, i, left_consumed);
    }
    if(left->iserror)
    {
        delete_node(left);
        consumed = 0;
        auto mynode = new node;
        mynode->identity = "deflist";
        mynode->iserror = true;
        mynode->error = "Error: expected name";
        mynode->position = i;
        mynode->errorpos = tokens[i].position;
        return mynode;
    }
    if(i+left_consumed+1 < tokens.size() and tokens[i+left_consumed].text == ",")
    {
        int tail_consumed = 0;
        auto tail = parse_deflist(tokens, i+left_consumed+1, tail_consumed);
        if(tail->iserror)
        {
            delete_node(tail);
            consumed = left_consumed;
            return left;
        }
        else
        {
            auto mynode = new node;
            mynode->identity = "deflist";
            mynode->text = ",";
            mynode->position = i+left_consumed;
            
            mynode->left = left;
            mynode->right = tail;
            left->parent = mynode;
            tail->parent = mynode;
            
            consumed = left_consumed+1+tail_consumed;
            return mynode;
        }
    }
    else
    {
        consumed = left_consumed;
        return left;
    }
}

node * parse_declaration(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    if(tokens[i].text == "var")
    {
        if(i+1 >= tokens.size())
        {
            auto mynode = new node;
            mynode->identity = "declaration";
            mynode->iserror = true;
            mynode->error = "Error: unexpected EOS during declaration";
            mynode->position = i;
            mynode->errorpos = tokens[i].endposition;
            consumed = 0;
            return mynode;
        }
        auto mynode = new node;
        mynode->identity = "declaration";
        mynode->text = "var";
        int deflist_consumed = 0;
        auto deflist = parse_deflist(tokens, i+1, deflist_consumed);
        if(!deflist->iserror)
        {
            if(i+1+deflist_consumed >= tokens.size())
            {
                auto mynode = new node;
                mynode->identity = "declaration";
                mynode->iserror = true;
                mynode->error = "Error: unexpected EOS during declaration";
                mynode->position = i+1+deflist_consumed-1;
                mynode->errorpos = tokens[i+1+deflist_consumed-1].endposition;
                consumed = 0;
                return mynode;
            }
            if(i+1+deflist_consumed < tokens.size() and tokens[i+1+deflist_consumed].text == ";")
            {
                mynode->right = deflist;
                mynode->right->parent = mynode;
                consumed = 1+deflist_consumed+1;
                return mynode;
            }
            mynode->iserror = true;
            mynode->error = "Error: expected \";\" after declaration";
            mynode->errorpos = tokens[i+1+deflist_consumed].position;
            consumed = 0;
            return mynode;
        }
        mynode->iserror = true;
        mynode->error = "Error: invalid declaration";
        mynode->errorpos = tokens[i+1].position;
        consumed = 0;
        return mynode;
    }
    else
    {
        auto mynode = new node;
        mynode->identity = "declaration";
        mynode->iserror = true;
        mynode->error = "Error: expected declaration";
        mynode->position = i;
        mynode->errorpos = tokens[i].position;
        consumed = 0;
        return mynode;
    }
}

const std::vector<std::string> binary_mutations = {
    "=", "+=", "-=", "*=", "/="
};

const std::vector<std::string> unary_mutations = {
    "++", "=="
};

node * parse_mutation(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    int name_consumed = 0;
    auto name = parse_name(tokens, i, name_consumed);
    if(name->iserror)
    {
        delete_node(name);
        
        auto mynode = new node;
        mynode->identity = "mutation";
        mynode->iserror = true;
        mynode->error = "Error: expected name";
        mynode->position = i;
        mynode->errorpos = tokens[i].position;
        consumed = 0;
        return mynode;
    }
    else if(i+name_consumed >= tokens.size())
    {
        delete_node(name);
        
        auto mynode = new node;
        mynode->identity = "mutation";
        mynode->iserror = true;
        mynode->error = "Error: unexpected end of stream when looking for assignment operation";
        mynode->position = i;
        mynode->errorpos = tokens[i].position + 1;
        consumed = 0;
        return mynode;
    }
    else
    {
        bool is_binary_mutation = false;
        bool is_unary_mutation = false;
        std::string found_mutation = "";
        for(auto m : binary_mutations)
        {
            if(m == tokens[i+name_consumed].text)
            {
                is_binary_mutation = true;
                found_mutation = m;
                break;
            }
        }
        if(!is_binary_mutation)
        {
            for(auto m : unary_mutations)
            {
                if(m == tokens[i+name_consumed].text)
                {
                    is_unary_mutation = true;
                    found_mutation = m;
                    break;
                }
            }
        }
        if(!is_binary_mutation and !is_unary_mutation)
        {
            delete_node(name);
            
            auto mynode = new node;
            mynode->identity = "mutation";
            mynode->iserror = true;
            mynode->error = "Error: unexpected symbol when looking for assignment operation";
            mynode->position = i+name_consumed;
            mynode->errorpos = tokens[i+name_consumed].position;
            consumed = 0;
            return mynode;
        }
        else if(is_binary_mutation and i+name_consumed+1 >= tokens.size())
        {
            delete_node(name);
            
            auto mynode = new node;
            mynode->identity = "mutation";
            mynode->iserror = true;
            mynode->error = "Error: unexpected end of stream when looking for assignment argument";
            mynode->position = i+name_consumed;
            mynode->errorpos = tokens[i+name_consumed].endposition;
            consumed = 0;
            
            return mynode;
        }
        else if(is_binary_mutation)
        {
            int expr_consumed = 0;
            auto expr = parse_expression(tokens, i+name_consumed+1, expr_consumed);
            if(expr->iserror)
            {
                delete_node(name);
                delete_node(expr);
                
                auto mynode = new node;
                mynode->identity = "mutation";
                mynode->iserror = true;
                mynode->error = "Error: expression expected in argument position of assignment operation";
                mynode->position = i+name_consumed+1;
                mynode->errorpos = tokens[i+name_consumed+1].position;
                consumed = 0;
                return mynode;
            }
            else
            {
                auto mynode = new node;
                mynode->identity = "mutation";
                mynode->text = found_mutation;
                mynode->position = i;
                mynode->left = name;
                mynode->right = expr;
                name->parent = mynode;
                expr->parent = mynode;
                consumed = name_consumed+1+expr_consumed;
                return mynode;
            }
        }
        else
        {
            auto mynode = new node;
            mynode->identity = "mutation";
            mynode->text = found_mutation;
            mynode->position = i;
            mynode->left = name;
            name->parent = mynode;
            consumed = name_consumed+1;
            return mynode;
        }
    }
}

node * parse_funcargs(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    int left_consumed = 0;
    auto left = parse_expression(tokens, i, left_consumed);
    if(left->iserror)
    {
        delete_node(left);
        auto mynode = new node;
        mynode->identity = "funcargs";
        mynode->iserror = true;
        mynode->error = "Error: no arguments in function argument list";
        mynode->errorpos = tokens[i].position;
        return mynode;
    }
    std::vector<node *> arguments = {left};
    int addon_total_consumed = 0;
    if(i+left_consumed+1 < tokens.size() and tokens[i+left_consumed].text == ",")
    {
        int addon_consumed = 1;
        auto addon = parse_expression(tokens, i+left_consumed+1, addon_consumed);
        while(!addon->iserror)
        {
            arguments.push_back(addon);
            addon_total_consumed += addon_consumed;
            
            if(i+left_consumed+1+addon_total_consumed >= tokens.size())
            {
                for(const auto & a : arguments)
                    delete a;
                arguments.clear();
                addon = nullptr;
                left = nullptr;
                
                auto mynode = new node;
                mynode->identity = "funcargs";
                mynode->iserror = true;
                mynode->error = "Error: unexpected end of stream while parsing argument list";
                mynode->errorpos = tokens[i+left_consumed+1+addon_total_consumed-1].endposition;
                return mynode;
            }
            
            if(tokens[i+left_consumed+1+addon_total_consumed].text == ")")
                break;
            else if(tokens[i+left_consumed+1+addon_total_consumed].text == ",")
            {
                addon = parse_expression(tokens, i+left_consumed+1+addon_total_consumed+1, addon_consumed);
                addon_total_consumed += 1+addon_consumed;
            }
            else
            {
                for(const auto & a : arguments)
                    delete a;
                arguments.clear();
                addon = nullptr;
                left = nullptr;
                
                auto mynode = new node;
                mynode->identity = "funcargs";
                mynode->iserror = true;
                mynode->error = "Error: unexpected symbol encountered while parsing function arguments";
                mynode->errorpos = tokens[i+left_consumed+1+addon_total_consumed].position;
                return mynode;
            }
        }
        if(addon->iserror)
            delete_node(addon);
    }
    
    int arraynodes = arguments.size();
    
    auto array = (node **)malloc(sizeof(node*)*arraynodes);
    auto mynode = new node;
    for(int i = 0; i < arraynodes; i++)
    {
        array[i] = arguments[i];
        array[i]->parent = mynode;
    }
    arguments.clear();
    
    mynode->identity = "funcargs";
    mynode->text = "()";
    mynode->position = i;
    mynode->nodearray = array;
    mynode->arraynodes = arraynodes;
    
    consumed = left_consumed+addon_total_consumed;
    
    return mynode;
}

node * parse_funccall(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    if(!is_name(tokens[i].text))
    {
        auto mynode = new node;
        mynode->identity = "funccall";
        mynode->iserror = true;
        mynode->error = "Error: excepted function name";
        mynode->errorpos = tokens[i].position;
        return mynode;
    }
    if(i+1 >= tokens.size())
    {
        auto mynode = new node;
        mynode->identity = "funccall";
        mynode->iserror = true;
        mynode->error = "Error: unexpected end of stream when expecting start of function argument list";
        mynode->errorpos = tokens[i].endposition;
        return mynode;
    }
    if(tokens[i+1].text != "(")
    {
        auto mynode = new node;
        mynode->identity = "funccall";
        mynode->iserror = true;
        mynode->error = "Error: unexpected symbol at start of function argument list";
        mynode->errorpos = tokens[i+1].position;
        return mynode;
    }
    if(i+2 >= tokens.size())
    {
        auto mynode = new node;
        mynode->identity = "funccall";
        mynode->iserror = true;
        mynode->error = "Error: unexpected end of stream when expecting contents or end of function argument list";
        mynode->errorpos = tokens[i+1].endposition;
        return mynode;
    }
    
    int funcargs_consumed = 0;
    auto funcargs = parse_funcargs(tokens, i+2, funcargs_consumed);
    
    if(funcargs->iserror)
    {
        delete_node(funcargs);
        if(tokens[i+2].text == ")")
        {
            auto mynode = new node;
            mynode->identity = "funccall";
            mynode->text = tokens[i].text;
            consumed = 3;
            return mynode;
        }
        else
        {
            auto mynode = new node;
            mynode->identity = "funccall";
            mynode->iserror = true;
            mynode->error = "Error: unexpected symbol when expecting end of empty function argument list";
            mynode->errorpos = tokens[i+2].position;
            return mynode;
        }
    }
    else
    {
        if(i+2+funcargs_consumed >= tokens.size())
        {
            auto mynode = new node;
            mynode->identity = "funccall";
            mynode->iserror = true;
            mynode->error = "Error: unexpected end of stream when expecting end of function argument list";
            mynode->errorpos = tokens[i+2+funcargs_consumed-1].endposition;
            return mynode;
        }
        else
        {
            auto mynode = new node;
            mynode->identity = "funccall";
            mynode->text = tokens[i].text;
            mynode->right = funcargs;
            funcargs->parent = mynode;
            consumed = 3+funcargs_consumed;
            return mynode;
        }
    }
}

node * parse_order(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    if(is_order(tokens[i].text))
    {
        auto mynode = new node;
        mynode->identity = "order";
        mynode->text = tokens[i].text;
        mynode->position = i;
        consumed = 1;
        return mynode;
    }
    else
    {
        auto mynode = new node;
        mynode->identity = "order";
        mynode->iserror = true;
        mynode->error = "Error: unexpected symbol where order expected";
        mynode->errorpos = tokens[i].position;
        mynode->position = i;
        return mynode;
    }
}

node * parse_instruction_bare(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    auto mynode = new node;
    mynode->identity = "instruction";
    mynode->position = i;
    
    int inner_instruction_consumed = 0;
    auto inner_instruction = parse_mutation(tokens, i, inner_instruction_consumed);
    
    if(inner_instruction->iserror)
    {
        delete_node(inner_instruction);
        inner_instruction_consumed = 0;
        inner_instruction = parse_funccall(tokens, i, inner_instruction_consumed);
        
        if(inner_instruction->iserror)
        {
            delete_node(inner_instruction);
            inner_instruction_consumed = 0;
            inner_instruction = parse_order(tokens, i, inner_instruction_consumed);
            
            if(inner_instruction->iserror)
            {
                delete_node(inner_instruction);
                
                mynode->iserror = true;
                mynode->error = "Error: expected instruction";
                mynode->errorpos = tokens[i].position;
                consumed = 0;
                return mynode;
            }
        }
    }
    
    if(i+inner_instruction_consumed >= tokens.size())
    {
        delete_node(inner_instruction);
        auto mynode = new node;
        mynode->identity = "instruction";
        mynode->iserror = true;
        mynode->error = "Error: unexpected end of stream while parsing instruction";
        mynode->position = i;
        mynode->errorpos = tokens[tokens.size()-1].endposition;
        consumed = 0;
        return mynode;
    }
    else
    {
        auto mynode = new node;
        mynode->identity = "instruction";
        mynode->position = i;
        mynode->right = inner_instruction;
        inner_instruction->parent = mynode;
        consumed = inner_instruction_consumed;
        return mynode;
    }

}

node * parse_instruction(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    int instruction_consumed = 0;
    auto instruction = parse_instruction_bare(tokens, i, instruction_consumed);
    if(instruction->iserror)
    {
        consumed = 0;
        return instruction;
    }
    else if(i+instruction_consumed >= tokens.size() or tokens[i+instruction_consumed].text != ";")
    {
        delete_node(instruction);
        auto mynode = new node;
        mynode->identity = "instruction";
        mynode->iserror = true;
        mynode->error = "Error: expected \";\" at end of instruction";
        mynode->position = i+instruction_consumed;
        mynode->errorpos = tokens[i+instruction_consumed-1].endposition+1;
        consumed = 0;
        return mynode;
    }
    else
    {
        consumed = instruction_consumed+1;
        return instruction;
    }
}

node * parse_statement(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    if(i+1 < tokens.size() and (tokens[i].text == "if" or tokens[i].text == "while" or tokens[i].text == "for") and tokens[i+1].text == "(")
    {
        if(tokens[i].text == "if")
            return parse_condition_if(tokens, i, consumed);
        else if(tokens[i].text == "while")
            return parse_condition_while(tokens, i, consumed);
        else if(tokens[i].text == "for")
            return parse_condition_for(tokens, i, consumed);
        else
            puts("Impossible error in condition parser."), exit(0);
    }
    else if(tokens[i].text == "var")
    {
        return parse_declaration(tokens, i, consumed);
    }
    else if(tokens[i].text == "{")
    {
        return parse_bigblock(tokens, i, consumed);
    }
    else if(tokens[i].text == ";")
    {
        auto mynode = new node;
        mynode->identity = "blankstatement";
        mynode->text = ";";
        mynode->position = i;
        consumed = 1;
        return mynode;
    }
    else
    {
        return parse_instruction(tokens, i, consumed);
        /*
        auto mynode = new node;
        mynode->identity = "statement";
        mynode->iserror = true;
        mynode->error = "Error: expected statement";
        mynode->position = i;
        consumed = 1;
        return mynode;
        */
    }
}

node * parse_statementlist(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    int consume_left, consume_right;
    consume_left = 0;
    consume_right = 0;
    auto lhs = parse_statement(tokens, i, consume_left);
    if(lhs->iserror)
    {
        consumed = 0;
        return lhs;
    }
    auto rhs = parse_statementlist(tokens, i+consume_left, consume_right);
    if(rhs->iserror)
    {
        delete_node(rhs);
        auto mynode = new node;
        mynode->identity = "statementlist";
        mynode->left = lhs;
        lhs->parent = mynode;
        
        consumed = consume_left;
        return lhs;
    }
    else
    {
        auto mynode = new node;
        mynode->identity = "statementlist";
        mynode->left = lhs;
        mynode->right = rhs;
        
        lhs->parent = mynode;
        rhs->parent = mynode;
        consumed = consume_right+consume_left;
        return mynode;
    }
}

node * parse_bigblock(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    if(tokens[i].text == "{")
    {
        auto mynode = new node;
        mynode->identity = "bigblock";
        mynode->text = "{}";
        mynode->position = i;
        
        int consume = 0;
        auto rhs = parse_statementlist(tokens, i+1, consume);
        
        if(rhs->iserror)
        {
            delete_node(rhs);
            if(tokens[i+1].text == "}")
            {
                consumed = 2;
                return mynode;
            }
            else
            {
                mynode->iserror = true;
                mynode->error = "Error: invalid statement in block or expected closing brace";
                mynode->errorpos = tokens[i+1].position;
                consumed = 0;
                return mynode;
            }
        }
        else if(i+1+consume >= tokens.size())
        {
            delete_node(rhs);
            mynode->iserror = true;
            mynode->error = "Error: unexpected eos when looking for closing brace";
            mynode->errorpos = tokens[tokens.size()-1].endposition;
            consumed = 0;
            return mynode;
        }
        else if(tokens[i+1+consume].text != "}")
        {
            delete_node(rhs);
            mynode->iserror = true;
            mynode->error = "Error: expected closing brace";
            mynode->errorpos = tokens[i+1+consume].position;
            consumed = 0;
            return mynode;
        }
        
        mynode->right = rhs;
        rhs->parent = mynode;
        
        consumed = consume+2;
        return mynode;
    }
    else
    {
        consumed = 0;
        auto mynode = new node;
        mynode->iserror = true;
        mynode->error = "Error: expected { at start of block";
        mynode->position = i;
        mynode->errorpos = tokens[i].position;
        if(verbose) puts("Block signalling error");
        return mynode;
    }
}

node * parse_block(const std::vector<token> & tokens, int i, int & consumed)
{
    if(i < 0 or i >= tokens.size()) return unexpected_eos(i);
    
    if(tokens[i].text == "{")
    {
        return parse_bigblock(tokens, i, consumed);
    }
    else if(tokens[i].text == ";")
    {
        auto mynode = new node;
        mynode->identity = "blankstatement";
        mynode->text = ";";
        mynode->position = i;
        consumed = 1;
        return mynode;
    }
    else
    {
        return parse_statement(tokens, i, consumed);
    }
}

node * fix_precedence(node * tree);

node * parse(const std::vector<token> & tokens)
{
    if(tokens.size() == 0) return nullptr;
    
    int consumed;
    auto tree = parse_statementlist(tokens, 0, consumed);
    
    if(tree->iserror)
    {
        return tree;
    }
    else if(consumed < tokens.size())
    {
        delete_tree(tree);
        auto ret = unexpected_eos(consumed);
        ret->error = "Error: unexpected symbol or beginning of invalid statement";
        ret->errorpos = tokens[consumed].position;
        return ret;
    }
    else if(consumed > tokens.size())
    {
        delete_tree(tree);
        auto ret = unexpected_eos(consumed);
        ret->error = "Error: parsing thinks it overran the token list";
        ret->errorpos = tokens[tokens.size()-1].endposition+(consumed-tokens.size())-1;
        return ret;
    }
    else
        return fix_precedence(tree);
}

// don't put right-associative operators in here
std::map<std::string, int> precedences =
{{"&&", 0},
 {"||", 0},
 {"and", 0},
 {"or", 0},
 {"==", 1},
 {"!=", 1},
 {">=", 1},
 {"<=", 1},
 { ">", 1},
 { "<", 1},
 { "+", 2},
 { "-", 2},
 { "*", 3},
 { "/", 3}};

node * fix_precedence(node * tree)
{
    if(tree == nullptr) return nullptr;
    while(precedences.count(tree->identity)
        and tree->right and !tree->right->iserror and tree->left and !tree->left->iserror // binary operations only
        and precedences.count(tree->right->identity)
        and precedences[tree->identity] == precedences[tree->right->identity])
    {
        // example:
        // 1+2+3
        //  +
        // 1 +
        //  2 3
        // need to rotate into:
        //   +
        //  + 3
        // 1 2
        
        auto lhs = tree;
        auto rhs = tree->right;
        
        lhs->right = rhs->left;
        lhs->right->parent = lhs;
        
        rhs->left = lhs;
        rhs->parent = lhs->parent;
        lhs->parent = rhs;
        
        tree = rhs;
    }
    
    tree->left = fix_precedence(tree->left);
    tree->right = fix_precedence(tree->right);
    
    return tree;
}

// struct interpreter_state {
//     std::map<std::string, double> variables;
// };
/*
std::map<std::string, double> variables;

double interpret(node * tree)
{
    if(tree == nullptr)
    {
        return puts("Error: empty expression"), 0;
    }
    if(tree->identity == "statementlist")
    {
        if(!tree->left)
        {
            puts("Internal Error: no left statement in statement list in AST");
            exit(0);
        }
        else
        {
            interpret(tree->left);
            if(tree->right)
                interpret(tree->right);
            return 0;
        }
    }
    if(tree->identity == "instruction")
    {
        if(!tree->right)
        {
            puts("Internal Error: empty instruction node in AST");
            exit(0);
        }
        else
        {
            interpret(tree->right);
            return 0;
        }
    }
    if(tree->identity == "mutation")
    {
        if(!tree->left)
        {
            puts("Internal Error: mutation has no left argument in AST");
            exit(0);
        }
        if(!variables.count(tree->left->text))
        {
            printf("Error: variable %s has not been declared\n", tree->left->text.data());
            return 0;
        }
        if(tree->right)
        {
            if(tree->text == "=")
            {
                variables[tree->left->text] = interpret(tree->right);
                return 0;
            }
            else if(tree->text == "+=")
            {
                variables[tree->left->text] += interpret(tree->right);
                return 0;
            }
            else if(tree->text == "-=")
            {
                variables[tree->left->text] -= interpret(tree->right);
                return 0;
            }
            else if(tree->text == "*=")
            {
                variables[tree->left->text] *= interpret(tree->right);
                return 0;
            }
            else if(tree->text == "/=")
            {
                variables[tree->left->text] /= interpret(tree->right);
                return 0;
            }
            else
            {
                puts("Internal Error: unknown mutation operation");
                exit(0);
            }
        }
    }
    if(tree->identity == "declaration")
    {
        if(!tree->right)
        {
            puts("Internal Error: declaration has no right argument in AST");
            exit(0);
        }
        if(tree->right->identity == "name")
        {
            variables[tree->right->text] = 0;
            return 0;
        }
        else if(tree->right->identity == "deflist")
        {
            interpret(tree->right);
            return 0;
        }
        else if(tree->right->identity == "compound_name")
        {
            interpret(tree->right);
            return 0;
        }
    }
    if(tree->identity == "deflist")
    {
        if(tree->right and tree->left)
        {
            if(tree->left->identity == "name")
                variables[tree->left->text] = 0;
            else
                interpret(tree->left);
            
            if(tree->right->identity == "name")
                variables[tree->right->text] = 0;
            else
                interpret(tree->right);
            
            return 0;
        }
        else
        {
            puts("Internal Error: deflist does not have two children");
            exit(0);
        }
    }
    if(tree->identity == "name")
    {
        if(variables.count(tree->text))
            return variables[tree->text];
        else
        {
            printf("Error: variable %s not yet declared\n", tree->text.data());
            return 0;
        }
    }
    if(tree->identity == "compound_name")
    {
        if(tree->left and tree->right)
        {
            if(variables.count(tree->left->text))
            {
                printf("Error: redefinition of variable %s\n", tree->left->text.data());
                return 0;
            }
            else
            {
                variables[tree->left->text] = interpret(tree->right);
                return 0;
            }
        }
        else
        {
            puts("Internal Error: compound name does not have two children");
            exit(0);
        }
    }
    if(tree->identity == "corevalue")
    {
        return atof(tree->text.data());
    }
    if(tree->identity == "condition_if")
    {
        auto temp = interpret(tree->nodearray[0]);
        if(temp)
            interpret(tree->nodearray[1]);
        else if(tree->arraynodes == 3)
            interpret(tree->nodearray[2]);
        return 0;
    }
    if(tree->identity == "condition_else")
    {
        interpret(tree->right);
        return 0;
    }
    if(tree->identity == "condition_while")
    {
        auto temp = interpret(tree->nodearray[0]);
        while(temp)
        {
            interpret(tree->nodearray[1]);
            temp = interpret(tree->nodearray[0]);
        }
        return 0;
    }
    if(tree->identity == "exp_paren")
    {
        return interpret(tree->right);
    }
    if(tree->identity == "bigblock")
    {
        interpret(tree->right);
        return 0;
    }
    if(tree->identity == "blankstatement")
    {
        return 0;
    }
    if(tree->identity == "binary_op")
    {
        if(tree->left and tree->left)
        {
            if(tree->text == "+")
                return interpret(tree->left) + interpret(tree->right);
            if(tree->text == "-")
                return interpret(tree->left) - interpret(tree->right);
            if(tree->text == "*")
                return interpret(tree->left) * interpret(tree->right);
            if(tree->text == "/")
                return interpret(tree->left) / interpret(tree->right);
            
            if(tree->text == "==")
                return interpret(tree->left) == interpret(tree->right);
            if(tree->text == "!=")
                return interpret(tree->left) != interpret(tree->right);
            if(tree->text == "<=")
                return interpret(tree->left) <= interpret(tree->right);
            if(tree->text == ">=")
                return interpret(tree->left) >= interpret(tree->right);
            if(tree->text == "<")
                return interpret(tree->left) < interpret(tree->right);
            if(tree->text == ">")
                return interpret(tree->left) > interpret(tree->right);
        }
        else
        {
            puts("Internal Error: binary operation name does not have two children");
            exit(0);
        }
    }
    if(tree->identity == "unary_op")
    {
        if(tree->right)
        {
            if(tree->text == "+")
                return interpret(tree->right);
            if(tree->text == "-")
                return -interpret(tree->right);
            if(tree->text == "!")
                return !interpret(tree->right);
        }
        else
        {
            puts("Internal Error: binary operation name does not have two children");
            exit(0);
        }
    }
    if(tree->identity == "funccall")
    {
        if(tree->text == "print")
        {
            if(tree->right and tree->right->arraynodes == 1)
            {
                printf("%f\n", interpret(tree->right->nodearray[0]));
            }
            else
            {
                printf("Error: wrong number of arguments to function 'print'\n");
            }
        }
        else
        {
            printf("Error: unknown function '%s'\n", tree->text.data());
        }
        return 0;
    }
    return printf("Error: unknown identifier %s<%s>\n", tree->identity.data(), tree->text.data()), 0;
}
*/
#include "bytecode.cpp"

void encode_u16(std::vector<uint8_t> * bytecode, uint16_t value)
{
    bytecode->push_back(((value>>(8*1)&0xFF)));
    bytecode->push_back(((value>>(8*0)&0xFF)));
}
void encode_u64(std::vector<uint8_t> * bytecode, uint64_t value)
{
    bytecode->push_back(((value>>(8*7)&0xFF)));
    bytecode->push_back(((value>>(8*6)&0xFF)));
    bytecode->push_back(((value>>(8*5)&0xFF)));
    bytecode->push_back(((value>>(8*4)&0xFF)));
    bytecode->push_back(((value>>(8*3)&0xFF)));
    bytecode->push_back(((value>>(8*2)&0xFF)));
    bytecode->push_back(((value>>(8*1)&0xFF)));
    bytecode->push_back(((value>>(8*0)&0xFF)));
}
void encode_double(std::vector<uint8_t> * bytecode, double value)
{
    uint64_t punned_value;
    memcpy(&punned_value, &value, sizeof(double));
    bytecode->push_back(((punned_value>>(8*7)&0xFF)));
    bytecode->push_back(((punned_value>>(8*6)&0xFF)));
    bytecode->push_back(((punned_value>>(8*5)&0xFF)));
    bytecode->push_back(((punned_value>>(8*4)&0xFF)));
    bytecode->push_back(((punned_value>>(8*3)&0xFF)));
    bytecode->push_back(((punned_value>>(8*2)&0xFF)));
    bytecode->push_back(((punned_value>>(8*1)&0xFF)));
    bytecode->push_back(((punned_value>>(8*0)&0xFF)));
}

struct stackinfo {
    std::vector<uint64_t> breaks;
    std::vector<uint64_t> continues;
};

void compile(node * tree, std::vector<uint8_t> * bytecode, stackinfo * jumpdata);

void compile_while(node * tree, std::vector<uint8_t> * bytecode, stackinfo * jumpdata)
{
    std::vector<uint8_t> conditionhead;
    std::vector<uint8_t> loopblock;
    
    conditionhead.push_back(SAVESCOPE);
    compile(tree->nodearray[0], &conditionhead, jumpdata);
    conditionhead.push_back(TRUTH);
    conditionhead.push_back(JLIF);
    
    stackinfo newjumpinfo;
    loopblock.push_back(OPENSCOPE);
    compile(tree->nodearray[1], &loopblock, &newjumpinfo);
    
    newjumpinfo.continues.push_back(loopblock.size());
    loopblock.push_back(BREAK);
    encode_u64(&loopblock, 0);
    
    uint64_t break_target = loopblock.size();
    loopblock.push_back(LOADSCOPE);
    
    for(auto break_addr : newjumpinfo.breaks)
    {
        // each break instruction is 9 bytes long
        uint64_t relative_target = break_target-break_addr;
        std::vector<uint8_t> temp;
        encode_u64(&temp, relative_target);
        for(int i = 0; i < 8; i++)
            loopblock[break_addr+i+1] = temp[i];
    }
    
    encode_u64(&conditionhead, break_target+9);
    
    uint64_t condition_size = conditionhead.size();
    if(condition_size >= 0x8000'0000'0000'0000)
    {
        puts("Internal error: condition of while loop is really fucking long and breaks integer arithmetic in the bytecode compiler.");
        exit(0);
    }
    
    int64_t continue_target = -static_cast<int64_t>(condition_size);
    for(auto continue_addr : newjumpinfo.continues)
    {
        if(condition_size+continue_addr >= 0x8000'0000'0000'0000
        or condition_size+continue_addr < condition_size
        or condition_size+continue_addr < continue_addr)
        {
            puts("Internal error: condition of while loop and continue instruction are too far apart to jump between.");
            exit(0);
        }
        // each break instruction is 9 bytes long
        // calculate
        int64_t relative_target = continue_target-continue_addr;
        uint64_t encoded_target = static_cast<uint64_t>(relative_target);
        
        std::vector<uint8_t> temp;
        encode_u64(&temp, encoded_target);
        for(int i = 0; i < 8; i++)
            loopblock[continue_addr+i+1] = temp[i];
    }
    
    vector_append(bytecode, conditionhead);
    vector_append(bytecode, loopblock);
}

void compile_for(node * tree, std::vector<uint8_t> * bytecode, stackinfo * jumpdata)
{
    std::vector<uint8_t> initblock;
    std::vector<uint8_t> continueblock;
    std::vector<uint8_t> conditionhead;
    std::vector<uint8_t> loopblock;
    
    compile(tree->nodearray[0], &initblock, jumpdata);
    
    compile(tree->nodearray[2], &continueblock, jumpdata);
    
    // jump over continue block
    if(continueblock.size()+3 < 0x8000)
    {
        initblock.push_back(JS);
        encode_u16(&initblock, continueblock.size()+3);
    }
    else if(continueblock.size()+9 < 0x8000'0000'0000'0000)
    {
        initblock.push_back(JL);
        encode_u64(&initblock, continueblock.size()+9);
    }
    else
    {
        puts("Internal error: the post-loop statement of a for loop is too long to jump over.");
        exit(0);
    }
    
    compile(tree->nodearray[1], &conditionhead, jumpdata);
    conditionhead.push_back(TRUTH);
    conditionhead.push_back(JLIF);
    
    stackinfo newjumpinfo;
    loopblock.push_back(SAVESCOPE);
    loopblock.push_back(OPENSCOPE);
    compile(tree->nodearray[3], &loopblock, &newjumpinfo);
    loopblock.push_back(EXITSCOPE);
    loopblock.push_back(LOADSCOPE);
    
    // jump back over loop and condition and continue block
    // condition is still 8 bytes shorter than it should be
    if(continueblock.size()+conditionhead.size()+8+loopblock.size() <= 0x8000)
    {
        int16_t distance = -static_cast<int16_t>(continueblock.size()+conditionhead.size()+8+loopblock.size());
        uint16_t encoded_target = static_cast<uint16_t>(distance);
        loopblock.push_back(JS);
        encode_u16(&loopblock, encoded_target);
    }
    else if(continueblock.size()+conditionhead.size()+8+loopblock.size() <= 0x8000'0000'0000'0000)
    {
        int64_t distance = -static_cast<uint64_t>(continueblock.size()+conditionhead.size()+8+loopblock.size());
        uint64_t encoded_target = static_cast<uint64_t>(distance);
        loopblock.push_back(JL);
        encode_u64(&loopblock, encoded_target);
    }
    else
    {
        puts("Internal error: the total length of a for loop is too long to jump over.");
        exit(0);
    }
    
    // condition jump
    if(loopblock.size()+9 < 0x8000'0000'0000'0000)
    {
        uint64_t distance = loopblock.size()+9;
        encode_u64(&conditionhead, distance);
    }
    else
    {
        puts("Internal error: the total length of a for loop is too long to jump over.");
        exit(0);
    }
    
    // rewrite target addresses of breaks
    uint64_t break_target = loopblock.size();
    // already know that loopblock.size() is less than u63_max because the condition jump was valid
    for(auto break_addr : newjumpinfo.breaks)
    {
        // each break instruction is 9 bytes long
        uint64_t relative_target = break_target-break_addr;
        std::vector<uint8_t> temp;
        encode_u64(&temp, relative_target);
        for(int i = 0; i < 8; i++)
            loopblock[break_addr+i+1] = temp[i];
    }
    
    //uint64_t continue_target = conditionhead.size() + continueblock.size();
    for(auto continue_addr : newjumpinfo.continues)
    {
        uint64_t distance = conditionhead.size() + continueblock.size() + continue_addr;
        if(distance >= 0x8000'0000'0000'0000)
        {
            puts("Internal error: condition of while loop and continue instruction are too far apart to jump between.");
            exit(0);
        }
        // each break instruction is 9 bytes long
        // calculate
        int64_t relative_target = -static_cast<int64_t>(distance);
        uint64_t encoded_target = static_cast<uint64_t>(relative_target);
        
        std::vector<uint8_t> temp;
        encode_u64(&temp, encoded_target);
        for(int i = 0; i < 8; i++)
            loopblock[continue_addr+i+1] = temp[i];
    }
    
    bytecode->push_back(OPENSCOPE);
    vector_append(bytecode, initblock);
    vector_append(bytecode, continueblock);
    vector_append(bytecode, conditionhead);
    vector_append(bytecode, loopblock);
    bytecode->push_back(EXITSCOPE);
}

void compile(node * tree, std::vector<uint8_t> * bytecode, stackinfo * jumpdata)
{
    if(tree == nullptr)
    {
        puts("Error: empty expression");
        return;
    }
    if(tree->identity == "statementlist")
    {
        if(!tree->left)
        {
            puts("Internal Error: no left statement in statement list in AST");
            exit(0);
        }
        else
        {
            compile(tree->left, bytecode, jumpdata);
            if(tree->right)
                compile(tree->right, bytecode, jumpdata);
            return;
        }
    }
    if(tree->identity == "instruction")
    {
        if(!tree->right)
        {
            puts("Internal Error: empty instruction node in AST");
            exit(0);
        }
        else
        {
            compile(tree->right, bytecode, jumpdata);
            return;
        }
    }
    if(tree->identity == "mutation")
    {
        if(!tree->left)
        {
            puts("Internal Error: assignment has no left argument in AST");
            exit(0);
        }
        if(tree->right)
        {
            compile(tree->right, bytecode, jumpdata);
            bytecode->push_back(BINAS);
            
            for(const auto & c : tree->left->text)
                bytecode->push_back(c);
            bytecode->push_back('\0');
            
            if(tree->text == "=")
                bytecode->push_back(ASSIGN);
            else if(tree->text == "+=")
                bytecode->push_back(MUTADD);
            else if(tree->text == "-=")
                bytecode->push_back(MUTSUB);
            else if(tree->text == "*=")
                bytecode->push_back(MUTMUL);
            else if(tree->text == "/=")
                bytecode->push_back(MUTDIV);
            else
            {
                puts("Internal Error: unknown binary assignment operation");
                exit(0);
            }
            return;
        }
        else
        {
            bytecode->push_back(UNAS);
            
            for(const auto & c : tree->left->text)
                bytecode->push_back(c);
            bytecode->push_back('\0');
            
            if(tree->text == "++")
                bytecode->push_back(INCREMENT);
            else if(tree->text == "--")
                bytecode->push_back(DECREMENT);
            else
            {
                puts("Internal Error: unknown unary assignment operation");
                exit(0);
            }
            return;
        }
    }
    if(tree->identity == "declaration")
    {
        if(!tree->right)
        {
            puts("Internal Error: declaration has no right argument in AST");
            exit(0);
        }
        if(tree->right->identity == "name")
        {
            bytecode->push_back(DECLARE);
            
            for(const auto & c : tree->right->text)
                bytecode->push_back(c);
            bytecode->push_back('\0');
        }
        else if(tree->right->identity == "deflist" or tree->right->identity == "compound_name")
        {
            compile(tree->right, bytecode, jumpdata);
        }
        return;
    }
    if(tree->identity == "deflist")
    {
        if(tree->right and tree->left)
        {
            if(tree->left->identity == "name")
            {
                bytecode->push_back(DECLARE);
                
                for(const auto & c : tree->left->text)
                    bytecode->push_back(c);
                bytecode->push_back('\0');
            }
            else
                compile(tree->left, bytecode, jumpdata);
            
            if(tree->right->identity == "name")
            {
                bytecode->push_back(DECLARE);
                
                for(const auto & c : tree->right->text)
                    bytecode->push_back(c);
                bytecode->push_back('\0');
            }
            else
                compile(tree->right, bytecode, jumpdata);
            
            return;
        }
        else
        {
            puts("Internal Error: deflist does not have two children");
            exit(0);
        }
    }
    if(tree->identity == "compound_name")
    {
        if(tree->left and tree->right)
        {
            compile(tree->right, bytecode, jumpdata);
            
            bytecode->push_back(DECLSET);
            
            for(const auto & c : tree->left->text)
                bytecode->push_back(c);
            bytecode->push_back('\0');
            
            return;
        }
        else
        {
            puts("Internal Error: compound name does not have two children");
            exit(0);
        }
    }
    if(tree->identity == "name")
    {
        bytecode->push_back(PUSHVAR);
        
        for(const auto & c : tree->text)
            bytecode->push_back(c);
        bytecode->push_back('\0');
        
        return;
    }
    if(tree->identity == "corevalue")
    {
        if(is_number(tree->text))
        {
            bytecode->push_back(PUSHVAL);
            
            double value = atof(tree->text.data());
            encode_double(bytecode, value);
        }
        else if(is_string(tree->text))
        {
            if(tree->text.length() >= 2 and tree->text[0] == '"' and tree->text[tree->text.size()-1] == '"')
            {
                bytecode->push_back(PUSHTEXT);
                
                for(uint64_t i = 1; i+1 < tree->text.size(); i++)
                    bytecode->push_back(tree->text[i]);
                bytecode->push_back('\0');
            }
            else
            {
                puts("Internal error: invalid format for literal string");
                exit(0);
            }
        }
        else
        {
            puts("Internal error: invalid format for literal value, not a number or a string");
            exit(0);
        }
        
        return;
    }
    if(tree->identity == "condition_if")
    {
        // conditional expression
        compile(tree->nodearray[0], bytecode, jumpdata);
        bytecode->push_back(TRUTH);
        
        if(tree->arraynodes == 3)
        {
            std::vector<uint8_t> mainblock;
            std::vector<uint8_t> elseblock;
            mainblock.push_back(OPENSCOPE);
            elseblock.push_back(OPENSCOPE);
            compile(tree->nodearray[1], &mainblock, jumpdata);
            compile(tree->nodearray[2], &elseblock, jumpdata);
            mainblock.push_back(EXITSCOPE);
            elseblock.push_back(EXITSCOPE);
            
            if(elseblock.size()+3 < 0x8000)
            {
                mainblock.push_back(JS); // jump short unconditional
                uint16_t distance = elseblock.size()+3;
                encode_u16(&mainblock, distance);
            }
            else if(elseblock.size()+9 < 0x8000'0000'0000'0000)
            {
                mainblock.push_back(JL); // jump long unconditional
                uint64_t distance = elseblock.size()+9;
                encode_u64(&mainblock, distance);
            }
            else
            {
                puts("Internal error: desired jump to generate from \"if\" statement has a distance of 2^63 bytes or more");
                exit(0);
            }
            
            if(mainblock.size()+3 < 0x8000) // 1+2 is the size of the jump we're adding if this is true
            {
                bytecode->push_back(JSIF); // jump short if false
                uint16_t distance = mainblock.size()+3;
                encode_u16(bytecode, distance);
            }
            else if(mainblock.size()+9 < 0x8000'0000'0000'0000)
            {
                bytecode->push_back(JLIF); // jump long if false
                uint64_t distance = mainblock.size()+9;
                encode_u64(bytecode, distance);
            }
            else
            {
                puts("Internal error: desired jump to generate from \"if\" statement has a distance of 2^63 bytes or more");
                exit(0);
            }
            
            vector_append(bytecode, mainblock);
            vector_append(bytecode, elseblock);
        }
        else if(tree->arraynodes == 2)
        {
            std::vector<uint8_t> mainblock;
            mainblock.push_back(OPENSCOPE);
            compile(tree->nodearray[1], &mainblock, jumpdata);
            mainblock.push_back(EXITSCOPE);
            
            // size of a short jump is 3 bytes
            if(mainblock.size()+3 < 0x8000) // 1+2 is the size of the jump we're adding if this is true
            {
                bytecode->push_back(JSIF); // jump short if false
                uint16_t distance = mainblock.size()+3;
                encode_u16(bytecode, distance);
            }
            else if(mainblock.size()+9 < 0x8000'0000'0000'0000) /// otherwise it's 1+8
            {
                bytecode->push_back(JLIF); // jump long if false
                uint64_t distance = mainblock.size()+9;
                encode_u64(bytecode, distance);
            }
            else
            {
                puts("Internal error: desired jump to generate from \"if\" statement has a distance of 2^63 bytes or more");
                exit(0);
            }
            vector_append(bytecode, mainblock);
        }
        else
        {
            puts("Internal error: wrong number of nodes under if condition in AST");
            exit(0);
        }
        
        return;
    }
    if(tree->identity == "condition_else")
    {
        compile(tree->right, bytecode, jumpdata);
        return;
    }
    if(tree->identity == "condition_while")
    {
        if(tree->arraynodes == 2)
        {
            compile_while(tree, bytecode, jumpdata);
            return;
        }
        else
        {
            puts("Internal error: wrong number of nodes under while condition in AST");
            exit(0);
        }
    }
    if(tree->identity == "condition_for")
    {
        if(tree->arraynodes == 4)
        {
            compile_for(tree, bytecode, jumpdata);
            return;
        }
        else
        {
            puts("Internal error: wrong number of nodes under for condition in AST");
            exit(0);
        }
    }
    if(tree->identity == "exp_paren")
    {
        compile(tree->right, bytecode, jumpdata);
        return;
    }
    if(tree->identity == "bigblock")
    {
        bytecode->push_back(OPENSCOPE);
        compile(tree->right, bytecode, jumpdata);
        bytecode->push_back(EXITSCOPE);
        return;
    }
    if(tree->identity == "blankstatement")
    {
        return;
    }
    if(tree->identity == "binary_op")
    {
        if(tree->left and tree->left)
        {
            compile(tree->left, bytecode, jumpdata);
            compile(tree->right, bytecode, jumpdata);
            bytecode->push_back(BINOP);
            
            if(tree->text == "+")
                bytecode->push_back(ADD);
            else if(tree->text == "-")
                bytecode->push_back(SUB);
            else if(tree->text == "*")
                bytecode->push_back(MUL);
            else if(tree->text == "/")
                bytecode->push_back(DIV);
            else if(tree->text == "==")
                bytecode->push_back(EQ);
            else if(tree->text == "!=")
                bytecode->push_back(NEQ);
            else if(tree->text == ">=")
                bytecode->push_back(LTE);
            else if(tree->text == "<=")
                bytecode->push_back(GTE);
            else if(tree->text == ">")
                bytecode->push_back(GT);
            else if(tree->text == "<")
                bytecode->push_back(LT);
            else if(tree->text == "&&" or tree->text == "and")
                bytecode->push_back(AND);
            else if(tree->text == "||" or tree->text == "or")
                bytecode->push_back(AND);
            else 
            {
                puts("Internal Error: unknown binary operation");
                puts(tree->text.data());
                exit(0);
            }
            return;
        }
        else
        {
            puts("Internal Error: binary operation does not have two children");
            exit(0);
        }
    }
    if(tree->identity == "unary_op")
    {
        if(tree->right)
        {
            compile(tree->right, bytecode, jumpdata);
            bytecode->push_back(UNOP);
            
            if(tree->text == "+")
                bytecode->push_back(POSITIVE);
            else if(tree->text == "-")
                bytecode->push_back(NEGATIVE);
            else if(tree->text == "!")
                bytecode->push_back(NEGATION);
            else 
            {
                puts("Internal Error: unknown unary operation");
                puts(tree->text.data());
                exit(0);
            }
            return;
        }
        else
        {
            puts("Internal Error: unary operation does not have two children");
            exit(0);
        }
    }
    if(tree->identity == "funccall")
    {
        if(tree->right)
        {
            for(int i = 0; i < tree->right->arraynodes; i++)
            {
                compile(tree->right->nodearray[i], bytecode, jumpdata);
            }
            bytecode->push_back(CALL);
            for(auto & c : tree->text)
                bytecode->push_back(c);
            bytecode->push_back(0x00);
            bytecode->push_back(tree->right->arraynodes);
        }
        else
        {
            bytecode->push_back(CALL);
            for(auto & c : tree->text)
                bytecode->push_back(c);
            bytecode->push_back(0x00);
            bytecode->push_back(0x00);
        }
        return;
    }
    if(tree->identity == "order")
    {
        if(!jumpdata)
        {
            puts("Error: break or continue outside of loop");
            exit(0);
        }
        if(tree->text == "break")
            jumpdata->breaks.push_back(bytecode->size());
        else if(tree->text == "continue")
            jumpdata->breaks.push_back(bytecode->size());
        else
        {
            puts("Internal error: unknown order");
            puts(tree->text.data());
            exit(0);
        }
        bytecode->push_back(BREAK);
        encode_u64(bytecode, 0);
        return;
    }
    printf("Error: unknown identifier %s<%s>\n", tree->identity.data(), tree->text.data());
    return;//exit(0);
}

void test(std::string str)
{
    printf("Case: %s\n", str.data());
    
    auto tokens = lex(str);
    printf("Lex: ");
    for(const auto & token : tokens)
        printf("'%s' ", token.text.data());
    puts("");
    
    printf("Parse: ");
    auto tree = parse(tokens);
    if(tree != nullptr)
    {
        while(tree != nullptr and tree->parent != nullptr)
            tree = tree->parent;
        if(tree->iserror)
        {
            puts(tree->error.data());
            puts(str.data());
            for(int i = 0; i < tree->errorpos; i++)
                    printf(" ");
            puts("^");
            puts("");
        }
        else
        {
            print_node(tree);
            puts("");
            printf("Running compiler:\n");
            progstate program;
            compile(tree, &program.bytecode, nullptr);
            printf("Output of compiler: %d bytes:\n", program.bytecode.size());
            int i = 0;
            for(const uint8_t & c : program.bytecode)
            {
                printf("%02X ", c);
                i++;
                if(i == 16)
                {
                    i = 0;
                    puts("");
                }
            }
            puts("");
            printf("Running program:\n");
            interpret(&program);
            puts("");
            program.reset();
            printf("Disassembly:\n");
            disassemble(&program);
            //printf("Interpreter state at exit:\n");
            //for(auto const& [name, var] : variables)
            //    printf("%s: %f\n", name.data(), var);
            
            puts("");
        }
    }
    else
        printf(" (No parse)\n\n");
}


int main()
{
    // for lexer
    {
        ops.push_back("&&");
        ops.push_back("||");
        
        //ops.push_back("and");
        //ops.push_back("or");
        
        ops.push_back("++");
        ops.push_back("--");
        
        ops.push_back("+=");
        ops.push_back("-=");
        ops.push_back("*=");
        ops.push_back("/=");
        
        ops.push_back("==");
        ops.push_back("!=");
        ops.push_back(">=");
        ops.push_back("<=");
        
        ops.push_back(">");
        ops.push_back("<");
        
        ops.push_back("+");
        ops.push_back("-");
        ops.push_back("*");
        ops.push_back("/");
        
        ops.push_back("!");
        
        ops.push_back("=");
        ops.push_back(",");
        
        ops.push_back(";");
        ops.push_back("{");
        ops.push_back("}");
        ops.push_back("(");
        ops.push_back(")");
    }
    
    test("var x = 2*4+1==9;");
    test("var x = 1+2*4==3||1;");
    test("var x = 3==2*4+1||0;");
    test("var x = 3==1+2*4;");
    test("var x = 1*2==3+4;");
    test("var x = 1+2==3*4;");
    test("var x = 1+2+3*4*5;");
    test("var x = 1*2*3+4+5;");
    test("var x = 1*2+3*4+5;");
    test("var x = 1+2*3+4*5;");
    test("var x = 1*2+3+4*5;");
    test("var x = 1+2*3*4+5;");
    test("var x = 1+2-3+4-5;");
    test("var x = 1+(2-3)+4-5;");
    test("var x = 1+2-(3+4)-5;");
    test("var x = 1+2-3+(4-5);");
    test("var x = 100-8+7-6-5;");
    test("var x = 100-8+7-(6-5);");
    test("var x = 100-(8+7)-6-5;");
    test("var x = 100-(8+7)-(6-5);");
    test("var x = 100-(8+(7-6)-5);");
    test("var x = +5;");
    test("var x = -5;");
    test("var x = 5-;");
    test("var x = *5;");
    test("var x = 5*;");
    test("var x = !5;");
    test("var x = !!5;");
    test("var x = !0;");
    test("var x = !!0;");
    test("var x = or 5;");
    test("var x = 5 $ r;");
    test("var x = 0 || 3==2*4+1;");
//     
    test("var x = 0 or or;");
    test("var x = 5;");
    test("var x =  ;");
    test("var x =;");
    test(" ");
    test("");
    test("var");
    test("var;");
    test("var x");
    test("0 or 3");
    test("0 or2");
    test("0or 2");
    test(" 5 ");
    test("if(--1);");
    test("if(1){if(2) var y = 2; else var x = 1;} else var z = 3;");
    test("var x, y = 1, z = x, w = y; x = 5;");
    test("var x, y, z, w;");
    test("var x = 1, y = x;");
    test("var x = 1, y = 1; x += 2; var z = x+y; z = z-1; y /= 2; var w = -z + !!y;");
    test("var x = 1; var y = 2;");
    test("var x, y; y = 2;");
    test("var x = 2, y; y = 2;");
    test("var x; var z; var w;");
    test("var x = 1; x += 2; x = 3;");
    test("print(1);");
    test("print(1, 2);");
    test("print();");
    test("func();");
    test("var y, speed, grav; grav = 1; while(y < 10) { speed += grav/2; y += speed; speed += grav/2; }");
    test("while(0){}");
    test("while(0){ }");
    test("while(0){;}");
    test("var y, speed, grav; grav = 1;");
    test("var y, speed, grav; grav = 1; while(y < 10) { speed += grav/2; y += speed; speed += grav/2; } print(y); print(speed);");
    test("while(0){break;}");
    test("while(0){print(1);}");
    test("var x = 1; print(x);");
    
    
    test("var lasty, y, speed, grav; grav = 1; while(y < 10) { speed += grav/2; lasty = y; y += speed; speed += grav/2; } print(lasty); print(speed - grav/2); print(y);");
    test("var x = 1; if(x)print(x);");
    test("var x = 0, y = 2; if(x)print(x);else print(y);");
    
    test("var x = 1, y = 0; while(y < 3) { print(x); var x = 2; print(x); y++; }");
    
    test("print(\"Hello, world!\");");
    test("var x = \"Hello, \" + \"world!\"; print(x);");
    
    test("for(var i = 0; i < 10; i++) print(i);");
    test("for(var i = 0; i < 10; {i++;}) print(i);");
    
    test(
"var x = 0, y = x;\n"
"var gravity = 1;\n"
"var vspeed;\n"
"vspeed = 0;\n"
"var lasty = y;\n"
"while(y < 10)\n"
"{\n"
"    vspeed += gravity/2;\n"
"    y += vspeed;\n"
"    vspeed += gravity/2;\n"
"    var y = 0;\n"
"}\n"
"print(lasty);\n"
"print(vspeed-gravity/2);\n"
"print(y);\n"
"print(!!y);\n"
"if(y > 10) print(\"Exceeded position limit\");"
"for(var i = 0; i < 4; i++) print(i);\n"
"for(var i = 0; i < y; {i++; y -= vspeed;})\n"
"{\n"
"    print(i);\n"
"    print(y);\n"
"}\n");
    
    /*
    test("2*3/4");
    test("2/3*4");
    
    test("2/(5+1)*2");
    test("2*(5+1)");
    */
}
