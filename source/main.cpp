#include <iostream>
#include <fstream>
#include <cstring>

#include "GLSbot.h"

int main()
{
    ::srand((u32) time(NULL));

    char buffer[128];
    auto auth = std::ifstream("./auth.txt");    // Contains 'Token: {Bot-Token}'
    auth.getline(buffer, 128);
    auto line = std::string_view(buffer, strlen(buffer) + 1);
    if (line.substr(0, 7) != "Token: ") std::__throw_invalid_argument("Token File invalid!");

    auto bot = GLSbot(line.substr(7));

    return 0;
}