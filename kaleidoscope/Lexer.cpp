#include "Lexer.hpp"

/// gettok - Return the next token from standard input.
int gettok() {
    static int LastChar = ' ';

    // Skip any whitespace.
    while (isspace(LastChar)) LastChar = getchar(); // cin >> ws;
    if (isalpha(LastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9]*
        IdentifierStr = LastChar;
        while (isalnum((LastChar = getchar()))) IdentifierStr += LastChar; //While is alphanumeric, keep reading characters and appending to IdentifierStr
        if (IdentifierStr == "def") return tok_def; //Check if is a definition, if so return the token for definition
        if (IdentifierStr == "extern") return tok_extern; //Check if is an extern, if so return the token for extern
        if (IdentifierStr == "if") return tok_if;
        if (IdentifierStr == "then") return tok_then;
        if (IdentifierStr == "else") return tok_else;
        if (IdentifierStr == "for") return tok_for;
        if (IdentifierStr == "in") return tok_in;
        if (IdentifierStr == "binary") return tok_binary;
        if (IdentifierStr == "unary") return tok_unary;
        if (IdentifierStr == "var") return tok_var;
        if (IdentifierStr == "exit") return tok_eof;
        return tok_identifier; //Otherwise, return the token for identifier, which means we have read an identifier that is not a keyword, and we will handle it as a normal identifier in the parser.
    }

    if (isdigit(LastChar) || LastChar == '.') {   // Number: [0-9.]+ if the last character is a digit or a dot, then we are starting to read a number
        std::string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getchar(); //Append the last character to the number string and read the next character
        } while (isdigit(LastChar) || LastChar == '.'); //Append while is a number or a dot
        NumVal = strtod(NumStr.c_str(), 0); //Convert the number string to a double and store it in NumVal
        return tok_number; //Return the token for number
    }

    if (LastChar == '#') {
        // Comment until end of line.
        do LastChar = getchar();
        while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
        if (LastChar != EOF) return gettok();
    }

    // Check for end of file.  Don't eat the EOF.
    if (LastChar == EOF) return tok_eof;

    // Otherwise, just return the character as its ascii value.
    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar; //Return ThisChar that is the ascii value of the character we just read, and set LastChar to the next character for the next call to gettok
}