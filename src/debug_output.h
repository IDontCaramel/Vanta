#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ast.h"
#include "token.h"

std::string tokenTypeName(TokenType type);
std::string formatTokens(const std::vector<Token>& tokens);
std::string formatAst(const std::shared_ptr<ProgramStmt>& program);
std::string formatAst(const ExprPtr& expression);
