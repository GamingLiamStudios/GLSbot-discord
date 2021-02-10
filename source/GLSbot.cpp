#include "GLSbot.h"
#include "util/types.h"

#include <iostream>
#include <string_view>

#include <cpr/cpr.h>
#include <simdjson/simdjson.h>
#include <zlib-ng.h>

GLSbot::GLSbot(std::string_view bot_token)
{
    auto res = cpr::Get(
      cpr::Url { "https://discord.com/api/v8/gateway/bot" },
      cpr::Header { { "Authorization", std::string("Bot ") + bot_token.data() } },
      cpr::VerifySsl(false));
    if (res.error)
    {
        std::cerr << "GetGatewayBot failed with error code " << (u16) res.error.code << ": "
                  << res.error.message << "\n";
        return;
    }

    if (res.header["content-type"] != "application/json")
        std::__throw_runtime_error("content-type unknown");

    simdjson::ondemand::parser parser;
    auto                       json = simdjson::padded_string(res.text);
    auto                       doc  = parser.iterate(json);

    auto uri = doc["url"].get_string();
    if (uri.error() != simdjson::error_code::SUCCESS)
    {
        std::cerr << "URI does not exist in JSON. Suspected failure\n";
        return;    // TODO: Print Error
    }

    std::string ws_url = std::string(uri.value());
    if (ws_url.back() != '/') ws_url.push_back('/');
    ws_url.append("?v=8&encoding=json");    // &compress?=zlib-stream

    ws = WebSocket(ws_url);

    /* TODO: Zlib Compression
        zng_stream stream;
        zng_inflateInit(&stream);
        char buffer[4096];
    */

    while (ws.connected || ws.iqueue_sizeapprox() != 0)
    {
        while (ws.iqueue_sizeapprox() == 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto inbound = ws.dump_iqueue();

        for (auto &frame : inbound)
        {
            if (frame.opcode != WebSocket::Opcode::text_frame) continue;
            auto str = std::string_view((char *) frame.payload_data, frame.payload_length);
            std::cout << str << "\n";
        }
    }
}