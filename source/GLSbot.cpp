#include "GLSbot.h"

#include <iostream>
#include <vector>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <cctype>
#include <string>

#include <cpr/cpr.h>
#include <fmt/format.h>
#include <json/json.hpp>

namespace
{
    inline std::string send_message(std::string_view token, std::string_view id, std::string &json)
    {
        auto response = cpr::Post(
          cpr::Url(fmt::format("https://discord.com/api/channels/{}/messages", id)),
          cpr::Header { { "Authorization", fmt::format("Bot {}", token) },
                        { "Content-Type", "application/json" } },
          cpr::VerifySsl(false),
          cpr::Body { json });

        auto parsed = nlohmann::json::parse(response.text);
        if (parsed.find("id") == parsed.end())
        {
            std::cout << "Failed to send message\n";
            std::cout << response.text << "\n";
            return "";
        }

        return parsed["id"].get<std::string>();
    }
    inline std::string
      send_response(std::string_view token, nlohmann::json &event_data, std::string &json)
    {
        auto channel_id = event_data["channel_id"].get<std::string_view>();
        return send_message(token, channel_id, json);
    }

    inline std::string create_dm(std::string_view token, std::string_view user_id)
    {
        std::string json     = fmt::format("{{\"recipient_id\": \"{}\"}}", user_id);
        auto        response = cpr::Post(
          cpr::Url("https://discord.com/api/users/@me/channels"),
          cpr::Header { { "Authorization", fmt::format("Bot {}", token) },
                        { "Content-Type", "application/json" } },
          cpr::VerifySsl(false),
          cpr::Body { json });

        return nlohmann::json::parse(response.text)["id"].get<std::string>();
    }
}    // namespace

void GLSbot::start(std::string_view token, std::string_view owner_user)
{
    if (gateway.connect(token) < 0) std::__throw_runtime_error("Failed to connect to gateway");
    std::cout << "Successfully Connected to Gateway\n";

    std::string                   owner_dm;
    std::vector<std::string_view> words;
    while (gateway.connected())
    {
        int incoming = gateway.get_incoming();
        for (int _ = 0; _ < incoming; _++)
        {
            auto [event_name, event_data] = gateway.next_event();
            // std::cout << event_name << "\n";
            // std::cout << event_data.dump(-1) << "\n";
            if (event_name == "READY")
            {
                std::string presence =
                  "{\"status\":\"online\",\"afk\":false,\"activities\":"
                  "[{\"name\":\"Your Screams\",\"type\":2,\"created_at\":";
                presence += std::to_string(std::time(0));
                presence += "}],\"since\":null}";
                gateway.send_event(discord::Gateway::PresenceUpdate, presence);
            }
            else if (event_name == "RESUMED")
            {
                if (!std::filesystem::exists("./cache.json"))
                {
                    std::cout << "Resume cache non-existant! Unable to properly resume bot\n";
                    gateway.disconnect();
                    gateway.connect(token);
                    break;
                }

                std::string json_file;
                auto        file = std::ifstream("./cache.json", std::ios::in | std::ios::ate);
                json_file.resize(file.tellg());
                file.seekg(0, std::ios::beg);
                file.read(json_file.data(), json_file.size());
                file.close();

                auto json = nlohmann::json::parse(json_file);
                guilds    = json["guilds"].get<std::vector<std::string>>();
                owner_id  = json["owner_id"].get<std::string>();

                owner_dm = create_dm(token, owner_id);

                std::string presence =
                  "{\"status\":\"online\",\"afk\":false,\"activities\":"
                  "[{\"name\":\"Your Screams\",\"type\":2,\"created_at\":";
                presence += std::to_string(std::time(0));
                presence += "}],\"since\":null}";
                gateway.send_event(discord::Gateway::PresenceUpdate, presence);
            }
            else if (event_name == "GUILD_CREATE")
            {
                std::string id = event_data["id"].get<std::string>();
                guilds.push_back(id);

                if (owner_id.empty() & !owner_user.empty())
                {
                    gateway.send_event(
                      discord::Gateway::RequestGuildMembers,
                      fmt::format(
                        "{{\"guild_id\":\"{}\",\"query\":\"{}\",\"limit\":100}}",
                        id,
                        owner_user.substr(0, owner_user.size() - 5)));
                }

                write_cache();
            }
            else if (event_name == "GUILD_MEMBERS_CHUNK")
            {
                auto users = event_data["members"].get<std::vector<nlohmann::json>>();
                if (users.size() == 0) continue;
                for (auto &user : users)
                {
                    if (
                      user["user"]["discriminator"].get<std::string_view>() ==
                      owner_user.substr(owner_user.size() - 4, 4))
                    {
                        owner_id = user["user"]["id"].get<std::string>();
                        write_cache();
                    }
                    // Can be used for other purposes
                }
            }
            else if (event_name == "MESSAGE_CREATE")
            {
                std::string author = fmt::format(
                  "{}#{}",
                  event_data["author"]["username"].get<std::string>(),
                  event_data["author"]["discriminator"].get<std::string>());
                std::string_view content = event_data["content"].get<std::string_view>();

                if (content.empty()) continue;
                if (content[0] != '!') continue;
                std::cout << author << ": " << content << "\n";

                // Split into individual words
                words.clear();
                size_t index = 1;    // +1 for escape-char
                while (index < content.size())
                {
                    size_t nindex = content.find(' ', index);
                    if (nindex == std::string_view::npos) nindex = content.size();
                    auto word = std::string_view(content.data() + index, nindex - index);
                    words.push_back(word);
                    index = nindex + 1;
                }
                if (words.size() == 0)
                    std::__throw_runtime_error("Something really fucked up. words.size == 0");

                if (words[0] == "ping")
                {
                    // TODO: Ping latency in ms
                    std::string post = "{\"content\":\"pong!\"}";
                    send_response(token, event_data, post);
                }
                if (words[0] == "shutdown" | words[0] == "die")
                {
                    std::string post;
                    if (words[0] == "shutdown")
                    {
                        if (event_data["author"]["id"].get<std::string>() == owner_id)
                            post = "{\"content\":\"Turning off the bot. Goodbye\"}";
                        else
                            post = "{\"content\":\"You do not have permission to do this\"}";
                    }
                    else
                    {
                        if (event_data["author"]["id"].get<std::string>() == owner_id)
                            post = "{\"content\":\"ok.\"}";
                        else
                            post = "{\"content\":\"no.\"}";
                    }
                    send_response(token, event_data, post);

                    if (event_data["author"]["id"].get<std::string>() == owner_id)
                    {
                        write_cache();
                        gateway.disconnect();
                    }
                }
                if (words[0] == "report")
                {
                    std::string post;
                    if (owner_id.empty())
                        post =
                          "{\"content\":\"This has been disabled. Please contact a Server "
                          "Moderator through your DMs instead\"}";
                    else if (words.size() == 1)
                        post =
                          "{\"content\":\"No problem to send. Please specify the problem after "
                          "'!report'\"}";
                    else
                        post = "{\"content\":\"Reported the problem to Bot Owner\"}";
                    send_response(token, event_data, post);

                    if (!owner_id.empty() && words.size() > 1)
                    {
                        if (owner_dm.empty()) owner_dm = create_dm(token, owner_id);
                        post = "{\"content\":\"Reported by " + author + ":\"}";
                        send_message(token, owner_dm, post);

                        post = "{\"content\":\"";
                        post += content.substr(8);
                        post += "\"}";
                        send_message(token, owner_dm, post);
                    }
                }
            }
        }
    }
}

void GLSbot::close()
{
    write_cache();
    gateway.close();
}

void GLSbot::write_cache()
{
    nlohmann::json json;
    json["owner_id"]     = owner_id;
    json["guilds"]       = guilds;
    std::string json_str = json.dump(-1);

    std::ofstream file;
    file.open("./cache.json");
    file.write(json_str.data(), json_str.size());
    file.flush();
    file.close();
}