#include "gateway.h"
#include "util/types.h"

#include <iostream>
#include <string_view>
#include <thread>
#include <chrono>
#include <string>

#include <cpr/cpr.h>
#include <simdjson/simdjson.h>
#include <zlib-ng.h>

namespace
{
    inline void dump_buffer(unsigned n, const unsigned char *buf)
    {
        int on_this_line = 0;
        while (n-- > 0)
        {
            fprintf(stderr, "%02X ", *buf++);
            on_this_line += 1;
            if (on_this_line == 16 || n == 0)
            {
                int i;
                fputs(" ", stderr);
                for (i = on_this_line; i < 16; i++) fputs(" ", stderr);
                for (i = on_this_line; i > 0; i--) fputc(isprint(buf[-i]) ? buf[-i] : '.', stderr);
                fputs("\n", stderr);
                on_this_line = 0;
            }
        }
    }
}    // namespace

discordAPI::Gateway::Gateway(std::string_view bot_token) : bot_token(bot_token)
{
    if (connect(bot_token) != 0) return;

    simdjson::padded_string      json;
    simdjson::ondemand::document doc;
    simdjson::ondemand::parser   parser;

    /* TODO: Zlib Compression
        zng_stream stream;
        zng_inflateInit(&stream);
        char buffer[4096];
    */

    std::thread heartbeat_thread;
    prev_seqnum = (u64) -1;
    ACK         = true;
    resume      = false;

    while (ws.connected || ws.iqueue_sizeapprox() != 0)
    {
        while (ws.iqueue_sizeapprox() == 0 && ws.connected)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto inbound = ws.dump_iqueue();

        for (auto &frame : inbound)
        {
            std::cout << "Frame Recieved\n";
            if (frame.opcode == WebSocket::Opcode::ping)
                ws.send_frame(WebSocket::Opcode::pong, frame.payload_data, frame.payload_length);
            if (frame.opcode == WebSocket::Opcode::connection_close) std::cout << "oh no\n";
            if (frame.opcode != WebSocket::Opcode::text_frame) continue;
            auto str = std::string_view((char *) frame.payload_data, frame.payload_length);

            std::cout << "Frame Data: " << str << "\n";

            json = simdjson::padded_string(str);
            doc  = parser.iterate(json);

            auto op = doc["op"].get_uint64();
            if (op.error() != simdjson::error_code::SUCCESS)
            {
                std::cerr << "OP does not exist in JSON. Suspected failure\n";
                std::cerr << simdjson::error_message(op.error()) << "\n";
                ws.close();
                break;
            }

            switch (op.value())
            {
            case (u16) Opcodes::Heartbeat:
            {
                std::string send = "{\"op\":1,\"d\":";
                if (prev_seqnum == (u64) -1)
                    send += "null";
                else
                    send += std::to_string(prev_seqnum);
                send += "}\n";

                ws.send_frame(WebSocket::Opcode::text_frame, (u8 *) send.data(), send.size());
            }
            break;
            case (u16) Opcodes::Hello:
            {
                auto interval = doc["d"].get_object()["heartbeat_interval"].get_uint64();
                if (interval.error() != simdjson::error_code::SUCCESS)
                {
                    std::cerr << "Heartbeat_interval does not exist. Suspected failure\n";
                    std::cerr << simdjson::error_message(interval.error()) << "\n";
                    ws.close();
                    break;
                }

                heartbeat_thread =
                  std::thread(&discordAPI::Gateway::heartbeat, this, interval.value());
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                if (!resume)
                {
                    std::cout << "Identify\n";
                    // TODO: OS detection
                    std::string identify = "{\"op\":2,\"d\":{\"token\":\"";
                    identify += bot_token;
                    identify +=
                      "\",\"intents\":23041,\"properties\":{\"$os\":\"windows\",\"$browser\":"
                      "\"GLSbot\",\"$device\":\"GLSbot\"},\"presence\":{\"status\":\"online\","
                      "\"afk\":false,\"activities\":[{\"name\":\"Your Screams\",\"type\":2,"
                      "\"created_at\":";
                    identify += std::to_string(std::time(0));
                    identify += "}]}}}";

                    ws.send_frame(
                      WebSocket::Opcode::text_frame,
                      (u8 *) identify.data(),
                      identify.size());
                    resume = true;
                }
                else
                {
                    std::cout << "Resume\n";
                    std::string resume = "{\"op\":6,\"d\":{\"token\":\"";
                    resume += bot_token;
                    resume += "\",\"session_id\":";
                    resume += session_id;
                    resume += ",\"seq\":";
                    if (prev_seqnum == (u64) -1)
                        resume += "null";
                    else
                        resume += std::to_string(prev_seqnum);
                    resume += "}}";

                    ws.send_frame(
                      WebSocket::Opcode::text_frame,
                      (u8 *) resume.data(),
                      resume.size());
                }
            }
            break;

            case (u16) Opcodes::HeartbeatACK: ACK = true; break;
            }
        }
    }

    heartbeat_thread.join();
}

int discordAPI::Gateway::connect(std::string_view bot_token)
{
    auto res = cpr::Get(
      cpr::Url { "https://discord.com/api/v8/gateway/bot" },
      cpr::Header { { "Authorization", std::string("Bot ") + bot_token.data() } },
      cpr::VerifySsl(false));
    if (res.error)
    {
        std::cerr << "GetGatewayBot failed with error code " << (u16) res.error.code << ": "
                  << res.error.message << "\n";
        return -1;
    }

    if (res.header["content-type"] != "application/json")
        std::__throw_runtime_error("content-type unknown");

    simdjson::ondemand::parser   parser;
    auto                         json = simdjson::padded_string(res.text);
    simdjson::ondemand::document doc  = parser.iterate(json);

    auto uri = doc["url"].get_string();
    if (uri.error() != simdjson::error_code::SUCCESS)
    {
        std::cerr << "URI does not exist in JSON. Suspected failure\n";
        std::cerr << simdjson::error_message(uri.error()) << "\n";
        return -1;
    }

    std::string ws_url = std::string(uri.value());
    if (ws_url.back() != '/') ws_url.push_back('/');
    ws_url.append("?v=8&encoding=json");    // &compress?=zlib-stream

    ws = WebSocket(ws_url);
    return 0;
}

void discordAPI::Gateway::heartbeat(u64 interval)
{
    auto        sleep = std::chrono::high_resolution_clock::now();
    std::string send;
    while (ws.connected)
    {
        send.clear();
        sleep += std::chrono::milliseconds(interval);

        if (!ACK)
        {
            std::cerr << "ACK not recieved\n";
            ws.close();
            // connect(bot_token);
            return;
        }

        ACK  = false;
        send = "{\"op\":1,\"d\":";
        if (prev_seqnum == (u64) -1)
            send += "null";
        else
            send += std::to_string(prev_seqnum);
        send += "}";

        ws.send_frame(WebSocket::Opcode::text_frame, (u8 *) send.data(), send.size());
        std::this_thread::sleep_until(sleep);
    }
}