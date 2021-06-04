#include <iostream>
#include <fstream>
#include <cstring>
#include <signal.h>

#include "GLSbot.h"

#include "websocket/ws.h"

GLSbot bot;

void sigint_callback(int signal)
{
    bot.close();
}

int main()
{
    ::srand((u32) time(NULL));

    char buffer[128];
    auto auth = std::ifstream("./auth.txt");    // Contains 'Token: {Bot-Token}'
    auth.getline(buffer, 128);
    auto line = std::string_view(buffer, strlen(buffer));
    if (line.substr(0, 7) != "Token: ") std::__throw_invalid_argument("Token File invalid!");

    signal(SIGINT, sigint_callback);
    bot.start(line.substr(7));

    return 0;
}