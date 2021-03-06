#include "gateway.h"
#include "util/types.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

#include <cpr/cpr.h>
#include <fmt/format.h>
// #include <zlib-ng.h>

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

bool discord::Gateway::connected()
{
    return ws.connected;
}

void discord::Gateway::close()
{
    nlohmann::json json;
    json["session_id"]   = session_id;
    json["prev_seqnum"]  = prev_seqnum.load();
    std::string json_str = json.dump(-1);

    std::ofstream file;
    file.open("./gcache.json");
    file.write(json_str.data(), json_str.size());
    file.flush();
    file.close();

    ws.close();

    resume = false;
    while (!next_events.empty()) next_events.pop();
}

void discord::Gateway::disconnect(u16 op)
{
    std::string post = "{\"d\":null,\"op\":" + std::to_string(op) + "}";
    ws.send_frame(WebSocket::Opcode::text_frame, (u8 *) post.data(), post.size());

    close();
}

int discord::Gateway::connect(std::string_view bot_token)
{
    nlohmann::json json;

    this->bot_token = bot_token;
    {
        auto res = cpr::Get(
          cpr::Url { "https://discord.com/api/gateway/bot" },
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

        json = nlohmann::json::parse(res.text);

        std::string ws_url = json["url"].get<std::string>();
        if (ws_url.back() != '/') ws_url.push_back('/');
        ws_url.append("?v=9&encoding=json");    // &compress?=zlib-stream

        ws = WebSocket(ws_url);
    }

    /* TODO: Zlib Compression
        zng_stream stream;
        zng_inflateInit(&stream);
        char buffer[4096];
    */
    prev_seqnum = (u64) -1;
    ACK         = true;
    resume      = false;

    u64 interval;

    if (std::filesystem::exists("./gcache.json"))
    {
        resume = true;

        std::ifstream file;
        file.open("./gcache.json", std::ios::ate | std::ios::in);
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string json_str(size, '0');
        file.read(json_str.data(), size);
        file.close();

        json        = nlohmann::json::parse(json_str);
        session_id  = json["session_id"];
        prev_seqnum = json["prev_seqnum"];
    }

    while (ws.connected || ws.iqueue_sizeapprox() != 0)
    {
        while (ws.iqueue_sizeapprox() == 0 && ws.connected)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto inbound = ws.dump_iqueue();

        bool gateway_success = false;

        for (auto &frame : inbound)
        {
            if (frame.opcode == WebSocket::Opcode::ping)
                ws.send_frame(
                  WebSocket::Opcode::pong,
                  frame.payload_data.get(),
                  frame.payload_length);
            if (frame.opcode == WebSocket::Opcode::connection_close)
            {
                std::cout << "oh no\n";
                dump_buffer(frame.payload_length, frame.payload_data.get());
                close();
            }
            if (frame.opcode != WebSocket::Opcode::text_frame) continue;

            std::string_view str =
              std::string_view((char *) frame.payload_data.get(), frame.payload_length);
            json = nlohmann::json::parse(str);

            if (json.find("op") == json.end())
            {
                std::cerr << "OP does not exist in JSON. Suspected failure\n";
                ws.close();
                break;
            }
            u64 op = json["op"].get<u64>();

            switch (op)
            {
            case (u16) Opcodes::Dispatch:
            {
                std::string    event_name = json["t"].get<std::string>();
                nlohmann::json event_data = json["d"];
                prev_seqnum               = json["s"].get<u64>();

                next_events.push({ event_name, event_data });

                if (event_name == "READY" || event_name == "RESUMED")
                {
                    if (event_name == "READY")
                        session_id = event_data["session_id"].get<std::string>();
                    std::thread(&discord::Gateway::heartbeat, this, interval).detach();
                    gateway_success = true;
                }
            }
            break;
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
                if (json["d"].find("heartbeat_interval") == json["d"].end())
                {
                    std::cerr << "Heartbeat_interval does not exist. Suspected failure\n";
                    ws.close();
                    break;
                }
                interval = json["d"]["heartbeat_interval"].get<u64>();

                if (!resume)
                {
                    // TODO: OS detection
                    std::string identify = "{\"op\":2,\"d\":{\"token\":\"";
                    identify += bot_token;
                    identify +=
                      "\",\"intents\":23041,\"compress\":false,\"properties\":{\"$os\":"
                      "\"windows\",\"$browser\":\"GLSbot\",\"$device\":\"GLSbot\"}}}";

                    ws.send_frame(
                      WebSocket::Opcode::text_frame,
                      (u8 *) identify.data(),
                      identify.size());

                    resume = true;
                }
                else
                {
                    std::string resume = "{\"op\":6,\"d\":{\"token\":\"";
                    resume += bot_token;
                    resume += "\",\"session_id\":\"";
                    resume += session_id;
                    resume += "\",\"seq\":";
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
            case (u16) Opcodes::InvalidSession:
            {
                if (json["d"].get<bool>())    // Unlikely
                {
                    std::string resume = "{\"op\":6,\"d\":{\"token\":\"";
                    resume += bot_token;
                    resume += "\",\"session_id\":\"";
                    resume += session_id;
                    resume += "\",\"seq\":";
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
                else
                {
                    // TODO: OS detection
                    std::string identify = "{\"op\":2,\"d\":{\"token\":\"";
                    identify += bot_token;
                    identify +=
                      "\",\"intents\":23041,\"compress\":false,\"properties\":{\"$os\":"
                      "\"windows\",\"$browser\":\"GLSbot\",\"$device\":\"GLSbot\"}}}";

                    ws.send_frame(
                      WebSocket::Opcode::text_frame,
                      (u8 *) identify.data(),
                      identify.size());

                    resume = true;
                }
            }
            break;
            case (u16) Opcodes::HeartbeatACK: ACK = true; break;
            default:
            {
                std::cout << "Unimplemented opcode in connect: " << op << "\n";
                std::cout << "json dump:" << json.dump(-1) << "\n";
                return -1;
            }
            break;
            }
        }
        if (gateway_success) return 1;
    }
    return -1;
}

int discord::Gateway::get_incoming()
{
    nlohmann::json json;
    int            incoming = 0;

    if (!next_events.empty()) return next_events.size();

    while (ws.iqueue_sizeapprox() == 0 && ws.connected)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!ws.connected) return 0;
    auto inbound = ws.dump_iqueue();

    for (auto &frame : inbound)
    {
        if (frame.opcode == WebSocket::Opcode::ping)
            ws.send_frame(WebSocket::Opcode::pong, frame.payload_data.get(), frame.payload_length);
        if (frame.opcode == WebSocket::Opcode::connection_close)
        {
            std::cout << "oh no\n";
            dump_buffer(frame.payload_length, frame.payload_data.get());
            close();
        }
        if (frame.opcode != WebSocket::Opcode::text_frame) continue;

        std::string_view str =
          std::string_view((char *) frame.payload_data.get(), frame.payload_length);
        // std::cout << str << "\n";
        json = nlohmann::json::parse(str);

        if (json.find("op") == json.end())
        {
            std::cerr << "OP does not exist in JSON. Suspected failure\n";
            ws.close();
            break;
        }
        u16 op = json["op"].get<u16>();

        switch (op)
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
        case (u16) Opcodes::Dispatch:
        {
            std::string    event_name = json["t"].get<std::string>();
            nlohmann::json event_data = json["d"];
            prev_seqnum               = json["s"].get<u64>();

            if (event_name == "Reconnect")
            {
                close();
                connect(bot_token);
                break;
            }

            next_events.push({ event_name, event_data });
            incoming++;
        }
        break;
        case (u16) Opcodes::InvalidSession:
        {
            close();
            connect(bot_token);
        }
        break;
        case (u8) Opcodes::HeartbeatACK: ACK = true; break;
        case (u8) Opcodes::Reconnect:
        {
            close();
            connect(bot_token);
        }
        break;
        default:
        {
            std::cout << "Unimplemented opcode in get_incoming: " << op << "\n";
            std::cout << "json dump:" << json.dump(-1) << "\n";
        }
        break;
        }
    }
    return incoming;
}

discord::Gateway::event discord::Gateway::next_event()
{
    auto event = next_events.front();
    next_events.pop();
    return event;
}

void discord::Gateway::send_event(send_opcodes op, std::string_view json)
{
    std::string send = fmt::format("{{\"op\": {}, \"d\": {}}}", (u8) op, json);
    ws.send_frame(WebSocket::Opcode::text_frame, (u8 *) send.data(), send.size());
}

void discord::Gateway::heartbeat(u64 interval)
{
    std::this_thread::sleep_for(
      std::chrono::milliseconds(u64(interval * (::rand() * 1.0 / RAND_MAX))));
    auto        sleep = std::chrono::high_resolution_clock::now();
    std::string send;
    while (ws.connected)
    {
        send.clear();

        if (!ACK)
        {
            std::cerr << "ACK not recieved. Reconnecting\n";
            disconnect(4009);
            connect(bot_token);
            return;
        }
        ACK = false;

        send = "{\"op\":1,\"d\":";
        if (prev_seqnum == (u64) -1)
            send += "null";
        else
            send += std::to_string(prev_seqnum);
        send += "}";
        ws.send_frame(WebSocket::Opcode::text_frame, (u8 *) send.data(), send.size());

        auto last = std::chrono::system_clock::now();
        {
            nlohmann::json json;
            json["session_id"]   = session_id;
            json["prev_seqnum"]  = prev_seqnum.load();
            std::string json_str = json.dump(-1);

            std::ofstream file;
            file.open("./gcache.json");
            file.write(json_str.data(), json_str.size());
            file.flush();
            file.close();
        }
        auto period = std::chrono::system_clock::now() - last;

        std::this_thread::sleep_for(std::chrono::milliseconds(interval) - period);
    }
}