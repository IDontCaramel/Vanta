#include "lexer.h"

#include <cctype>
#include <unordered_map>

namespace {

const std::unordered_map<std::string, TokenType> kKeywords = {
    {"var", TokenType::VAR},
    {"func", TokenType::FUNC},
    {"class", TokenType::CLASS},
    {"return", TokenType::RETURN},
    {"if", TokenType::IF},
    {"else", TokenType::ELSE},
    {"while", TokenType::WHILE},
    {"for", TokenType::FOR},
    {"break", TokenType::BREAK},
    {"continue", TokenType::CONTINUE},
    {"new", TokenType::NEW},
    {"this", TokenType::THIS},
    {"try", TokenType::TRY},
    {"catch", TokenType::CATCH},
    {"finally", TokenType::FINALLY},
    {"throw", TokenType::THROW},
    {"extends", TokenType::EXTENDS},
    {"true", TokenType::TRUE_},
    {"false", TokenType::FALSE_},
    {"null", TokenType::NULL_},
};

}  // namespace

Lexer::Lexer(std::string sourceCode) : source(std::move(sourceCode)) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!isAtEnd()) {
        skipWhitespaceAndComments();
        if (isAtEnd()) {
            break;
        }

        const char ch = peek();
        const int tokenLine = line;
        const int tokenColumn = column;

        if (std::isdigit(static_cast<unsigned char>(ch))) {
            tokens.push_back(scanNumber());
            continue;
        }

        if (ch == '$' && (peek(1) == '"' || peek(1) == '\'')) {
            advance();
            tokens.push_back(scanString(advance(), true));
            continue;
        }

        if (ch == '"' || ch == '\'') {
            tokens.push_back(scanString(advance(), false));
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
            tokens.push_back(scanIdentifierOrKeyword());
            continue;
        }

        advance();
        switch (ch) {
            case '+':
                if (match('=')) {
                    tokens.emplace_back(TokenType::PLUS_ASSIGN, "+=", tokenLine, tokenColumn, 2);
                } else {
                    tokens.emplace_back(TokenType::PLUS, "+", tokenLine, tokenColumn, 1);
                }
                break;
            case '-':
                if (match('=')) {
                    tokens.emplace_back(TokenType::MINUS_ASSIGN, "-=", tokenLine, tokenColumn, 2);
                } else if (match('>')) {
                    tokens.emplace_back(TokenType::TYPE_ARROW, "->", tokenLine, tokenColumn, 2);
                } else {
                    tokens.emplace_back(TokenType::MINUS, "-", tokenLine, tokenColumn, 1);
                }
                break;
            case '*':
                tokens.emplace_back(TokenType::STAR, "*", tokenLine, tokenColumn, 1);
                break;
            case '/':
                tokens.emplace_back(TokenType::SLASH, "/", tokenLine, tokenColumn, 1);
                break;
            case '%':
                tokens.emplace_back(TokenType::PERCENT, "%", tokenLine, tokenColumn, 1);
                break;
            case '=':
                if (match('=')) {
                    tokens.emplace_back(TokenType::EQ, "==", tokenLine, tokenColumn, 2);
                } else if (match('>')) {
                    tokens.emplace_back(TokenType::ARROW, "=>", tokenLine, tokenColumn, 2);
                } else {
                    tokens.emplace_back(TokenType::ASSIGN, "=", tokenLine, tokenColumn, 1);
                }
                break;
            case '!':
                if (match('=')) {
                    tokens.emplace_back(TokenType::NEQ, "!=", tokenLine, tokenColumn, 2);
                } else {
                    tokens.emplace_back(TokenType::NOT, "!", tokenLine, tokenColumn, 1);
                }
                break;
            case '<':
                if (match('=')) {
                    tokens.emplace_back(TokenType::LTE, "<=", tokenLine, tokenColumn, 2);
                } else {
                    tokens.emplace_back(TokenType::LT, "<", tokenLine, tokenColumn, 1);
                }
                break;
            case '>':
                if (match('=')) {
                    tokens.emplace_back(TokenType::GTE, ">=", tokenLine, tokenColumn, 2);
                } else {
                    tokens.emplace_back(TokenType::GT, ">", tokenLine, tokenColumn, 1);
                }
                break;
            case '&':
                if (match('&')) {
                    tokens.emplace_back(TokenType::AND, "&&", tokenLine, tokenColumn, 2);
                } else {
                    tokens.emplace_back(TokenType::ERROR_, "&", tokenLine, tokenColumn, 1);
                }
                break;
            case '|':
                if (match('|')) {
                    tokens.emplace_back(TokenType::OR, "||", tokenLine, tokenColumn, 2);
                } else if (match('>')) {
                    tokens.emplace_back(TokenType::PIPE, "|>", tokenLine, tokenColumn, 2);
                } else {
                    tokens.emplace_back(TokenType::ERROR_, "|", tokenLine, tokenColumn, 1);
                }
                break;
            case '(':
                tokens.emplace_back(TokenType::LPAREN, "(", tokenLine, tokenColumn, 1);
                break;
            case ')':
                tokens.emplace_back(TokenType::RPAREN, ")", tokenLine, tokenColumn, 1);
                break;
            case '{':
                tokens.emplace_back(TokenType::LBRACE, "{", tokenLine, tokenColumn, 1);
                break;
            case '}':
                tokens.emplace_back(TokenType::RBRACE, "}", tokenLine, tokenColumn, 1);
                break;
            case '[':
                tokens.emplace_back(TokenType::LBRACKET, "[", tokenLine, tokenColumn, 1);
                break;
            case ']':
                tokens.emplace_back(TokenType::RBRACKET, "]", tokenLine, tokenColumn, 1);
                break;
            case '.':
                tokens.emplace_back(TokenType::DOT, ".", tokenLine, tokenColumn, 1);
                break;
            case ',':
                tokens.emplace_back(TokenType::COMMA, ",", tokenLine, tokenColumn, 1);
                break;
            case ';':
                tokens.emplace_back(TokenType::SEMICOLON, ";", tokenLine, tokenColumn, 1);
                break;
            case ':':
                tokens.emplace_back(TokenType::COLON, ":", tokenLine, tokenColumn, 1);
                break;
            default:
                tokens.emplace_back(TokenType::ERROR_, std::string(1, ch), tokenLine, tokenColumn, 1);
                break;
        }
    }

    tokens.emplace_back(TokenType::EOF_, "", line, column, 1);
    return tokens;
}

char Lexer::peek(int offset) const {
    if (pos + static_cast<std::size_t>(offset) >= source.size()) {
        return '\0';
    }
    return source[pos + static_cast<std::size_t>(offset)];
}

char Lexer::advance() {
    const char ch = peek();
    ++pos;
    if (ch == '\n') {
        ++line;
        column = 1;
    } else {
        ++column;
    }
    return ch;
}

bool Lexer::match(char expected) {
    if (peek() != expected) {
        return false;
    }
    advance();
    return true;
}

bool Lexer::isAtEnd() const {
    return pos >= source.size();
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        const char ch = peek();
        if (ch == ' ' || ch == '\r' || ch == '\t' || ch == '\n') {
            advance();
            continue;
        }

        if (ch == '#') {
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
            continue;
        }

        break;
    }
}

Token Lexer::makeToken(TokenType type, const std::string& lexeme, int tokenLine, int tokenColumn,
                       int tokenLength) const {
    return Token(type, lexeme, tokenLine, tokenColumn, tokenLength);
}

Token Lexer::scanNumber() {
    const int tokenLine = line;
    const int tokenColumn = column;
    std::string lexeme;

    while (std::isdigit(static_cast<unsigned char>(peek()))) {
        lexeme += advance();
    }

    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        lexeme += advance();
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            lexeme += advance();
        }
    }

    return makeToken(TokenType::NUMBER, lexeme, tokenLine, tokenColumn);
}

Token Lexer::scanString(char quote, bool interpolation) {
    const int tokenLine = line;
    const int tokenColumn = column - (interpolation ? 2 : 1);
    std::string value;

    while (!isAtEnd() && peek() != quote) {
        if (peek() == '\\') {
            advance();
            const char escaped = advance();
            switch (escaped) {
                case 'n':
                    value += '\n';
                    break;
                case 'r':
                    value += '\r';
                    break;
                case 't':
                    value += '\t';
                    break;
                case '\\':
                    value += '\\';
                    break;
                case '"':
                    value += '"';
                    break;
                case '\'':
                    value += '\'';
                    break;
                case '{':
                    value += '{';
                    break;
                case '}':
                    value += '}';
                    break;
                default:
                    value += escaped;
                    break;
            }
        } else {
            value += advance();
        }
    }

    if (!isAtEnd()) {
        advance();
    }

    const int tokenLength = column - tokenColumn;
    return makeToken(interpolation ? TokenType::INTERP_STRING : TokenType::STRING, value,
                     tokenLine, tokenColumn, tokenLength);
}

Token Lexer::scanIdentifierOrKeyword() {
    const int tokenLine = line;
    const int tokenColumn = column;
    std::string lexeme;

    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
        lexeme += advance();
    }

    const auto it = kKeywords.find(lexeme);
    const TokenType type = it == kKeywords.end() ? TokenType::IDENTIFIER : it->second;
    return makeToken(type, lexeme, tokenLine, tokenColumn);
}
