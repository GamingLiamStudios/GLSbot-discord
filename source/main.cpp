#include <iostream>
#include <chrono>
#include <thread>

#include "websocket/ws.h"

int main()
{
    ::srand((u32) time(NULL));
    auto        ws   = WebSocket("wss://echo.websocket.org/");
    std::string text = "Ping-Pong Test";
    ws.send_frame(WebSocket::Opcode::text_frame, (u8 *) text.data(), text.size());

    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "\n";

    ws.send_frame(WebSocket::Opcode::connection_close, nullptr, 0);
    ws.close();

    auto iqueue = ws.dump_iqueue();

    for (auto &frame : iqueue)
    {
        std::cout << "Frame Recieved\n\tOpcode: " << std::to_string((u8) frame.opcode) << "\n";
        switch (frame.opcode)
        {
        case WebSocket::Opcode::text_frame:
            std::cout << "\tPayload: \""
                      << std::string_view((char *) frame.payload_data, frame.payload_length)
                      << "\"\n";
            break;
        }
    }

    std::cout << "Closed without error\n";
    return 0;
}