#pragma once

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

int runCli(const std::vector<std::string>& args, std::istream& in = std::cin,
           std::ostream& out = std::cout, std::ostream& err = std::cerr);
