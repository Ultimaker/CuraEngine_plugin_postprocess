#include "cura/plugins/slots/broadcast/v0/broadcast.grpc.pb.h"
#include "cura/plugins/slots/broadcast/v0/broadcast.pb.h"
#include "cura/plugins/slots/handshake/v0/handshake.grpc.pb.h"
#include "cura/plugins/slots/handshake/v0/handshake.pb.h"
#include "cura/plugins/slots/postprocess/v0/postprocess.grpc.pb.h"
#include "cura/plugins/slots/postprocess/v0/postprocess.pb.h"
#include "plugin/cmdline.h" // Custom command line argument definitions

#include <agrpc/asio_grpc.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>
#include <fmt/format.h> // Formatting library
#include <range/v3/to_container.hpp>
#include <range/v3/view/drop_last.hpp>
#include <range/v3/view/take_last.hpp>
#include <range/v3/view/tokenize.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h> // Logging library

#include <docopt/docopt.h> // Library for parsing command line arguments
#include <google/protobuf/empty.pb.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <map>
#include <optional>
#include <queue>
#include <regex>
#include <thread>

struct PluginMetadata
{
    std::string plugin_name{ plugin::cmdline::NAME };
    std::string slot_version{ "0.1.0-alpha.1" };
    std::string plugin_version{ plugin::cmdline::VERSION };
};

static const PluginMetadata metadata{};


int main(int argc, const char** argv)
{
    spdlog::set_level(spdlog::level::debug);
    constexpr bool show_help = true;
    const std::map<std::string, docopt::value> args
        = docopt::docopt(fmt::format(plugin::cmdline::USAGE, plugin::cmdline::NAME), { argv + 1, argv + argc }, show_help, plugin::cmdline::VERSION_ID);

    std::unique_ptr<grpc::Server> server;

    grpc::ServerBuilder builder;
    agrpc::GrpcContext grpc_context{ builder.AddCompletionQueue() };
    builder.AddListeningPort(fmt::format("{}:{}", args.at("--address").asString(), args.at("--port").asString()), grpc::InsecureServerCredentials());

    cura::plugins::slots::handshake::v0::HandshakeService::AsyncService handshake_service;
    builder.RegisterService(&handshake_service);

    cura::plugins::slots::broadcast::v0::BroadcastService::AsyncService broadcast_service;
    builder.RegisterService(&broadcast_service);

    cura::plugins::slots::postprocess::v0::PostprocessModifyService::AsyncService service;
    builder.RegisterService(&service);

    server = builder.BuildAndStart();

    // Start the handshake process
    boost::asio::co_spawn(
        grpc_context,
        [&]() -> boost::asio::awaitable<void>
        {
            while (true)
            {
                grpc::ServerContext server_context;

                cura::plugins::slots::handshake::v0::CallRequest request;
                grpc::ServerAsyncResponseWriter<cura::plugins::slots::handshake::v0::CallResponse> writer{ &server_context };
                co_await agrpc::request(
                    &cura::plugins::slots::handshake::v0::HandshakeService::AsyncService::RequestCall,
                    handshake_service,
                    server_context,
                    request,
                    writer,
                    boost::asio::use_awaitable);
                spdlog::info("Received handshake request");
                spdlog::info("Slot ID: {}, version_range: {}", static_cast<int>(request.slot_id()), request.version_range());

                cura::plugins::slots::handshake::v0::CallResponse response;
                response.set_plugin_name(metadata.plugin_name);
                response.set_plugin_version(metadata.plugin_version);
                response.set_slot_version(metadata.slot_version);
                response.set_plugin_version(metadata.plugin_version);
                response.mutable_broadcast_subscriptions()->Add("BroadcastSettings");

                co_await agrpc::finish(writer, response, grpc::Status::OK, boost::asio::use_awaitable);
            }
        },
        boost::asio::detached);

    // Listen to the Broadcast channel
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> settings;
    boost::asio::co_spawn(
        grpc_context,
        [&]() -> boost::asio::awaitable<void>
        {
            while (true)
            {
                grpc::ServerContext server_context;
                cura::plugins::slots::broadcast::v0::BroadcastServiceSettingsRequest request;
                grpc::ServerAsyncResponseWriter<google::protobuf::Empty> writer{ &server_context };
                co_await agrpc::request(
                    &cura::plugins::slots::broadcast::v0::BroadcastService::AsyncService::RequestBroadcastSettings,
                    broadcast_service,
                    server_context,
                    request,
                    writer,
                    boost::asio::use_awaitable);
                const google::protobuf::Empty response{};
                co_await agrpc::finish(writer, response, grpc::Status::OK, boost::asio::use_awaitable);

                auto c_uuid = server_context.client_metadata().find("cura-engine-uuid");
                if (c_uuid == server_context.client_metadata().end())
                {
                    spdlog::warn("cura-engine-uuid not found in client metadata");
                    continue;
                }
                const std::string client_metadata = std::string{ c_uuid->second.data(), c_uuid->second.size() };

                // We create a new settings map for this uuid
                std::unordered_map<std::string, std::string> uuid_settings;

                // We insert all the settings from the request to the uuid_settings map
                for (const auto& [key, value] : request.global_settings().settings())
                {
                    uuid_settings.emplace(key, value);
                    spdlog::info("Received setting: {} = {}", key, value);
                }

                // We save the settings for this uuid in the global settings map
                settings[client_metadata] = uuid_settings;
            }
        },
        boost::asio::detached);


    // Start the plugin modify process
    std::unordered_map<std::string, std::queue<std::string>> layers;
    boost::asio::co_spawn(
        grpc_context,
        [&]() -> boost::asio::awaitable<void>
        {
            while (true)
            {
                grpc::ServerContext server_context;
                cura::plugins::slots::postprocess::v0::CallRequest request;
                grpc::ServerAsyncResponseWriter<cura::plugins::slots::postprocess::v0::CallResponse> writer{ &server_context };
                co_await agrpc::request(
                    &cura::plugins::slots::postprocess::v0::PostprocessModifyService::AsyncService::RequestCall,
                    service,
                    server_context,
                    request,
                    writer,
                    boost::asio::use_awaitable);
                cura::plugins::slots::postprocess::v0::CallResponse response;

                auto c_uuid = server_context.client_metadata().find("cura-engine-uuid");
                if (c_uuid == server_context.client_metadata().end())
                {
                    spdlog::warn("cura-engine-uuid not found in client metadata");
                    continue;
                }
                std::string client_metadata = std::string{ c_uuid->second.data(), c_uuid->second.size() };
                auto jerk_enabled = settings[client_metadata].at("jerk_enabled") == "True";
                spdlog::info("jerk_enabled: {} for {}", jerk_enabled, client_metadata);

                grpc::Status status = grpc::Status::OK;
                try
                {
                    layers[client_metadata].push(request.gcode_word());
                    if (layers[client_metadata].size() > 1)
                    {
                        std::string layer_string = layers[client_metadata].front();

                        auto lines = layer_string | ranges::views::tokenize(std::regex{ "\n" }, -1)
                                   | ranges::views::transform(
                                         [](auto&& line)
                                         {
                                             return std::string{ line };
                                         })
                                   | ranges::to<std::vector>;

                        if (lines.size() > 2)
                        {
                            auto second_last_line = lines.end()[-1];
                            spdlog::info("second_last_line: {}", second_last_line);
                            response.set_gcode_word(fmt::format("; last line of layer before: {} and jerk_enabled: {}\n{}", second_last_line, jerk_enabled, request.gcode_word()));
                        }
                        else
                        {
                            response.set_gcode_word(fmt::format("; first layer and jerk_enabled: {}\n{}", jerk_enabled, request.gcode_word()));
                        }
                        layers[client_metadata].pop();
                    }
                    else
                    {
                        response.set_gcode_word(layers[client_metadata].back());
                    }
                }
                catch (const std::exception& e)
                {
                    spdlog::error("Error: {}", e.what());
                    status = grpc::Status(grpc::StatusCode::INTERNAL, static_cast<std::string>(e.what()));
                }

                co_await agrpc::finish(writer, response, status, boost::asio::use_awaitable);
            }
        },
        boost::asio::detached);
    grpc_context.run();

    server->Shutdown();
}