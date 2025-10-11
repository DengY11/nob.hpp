#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "executor/executor.hpp"

int main() {
    std::string input = "Hello, World!";
    lex(input);
    parse(input);
    execute(input);
    return 0;
}