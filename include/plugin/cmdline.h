#ifndef PLUGIN_CMDLINE_H
#define PLUGIN_CMDLINE_H

#include <string>
#include <string_view>

#include <fmt/compile.h>

namespace plugin::cmdline
{

constexpr std::string_view NAME = "CuraEngine plugin postprocess";
constexpr std::string_view VERSION = "0.1.0-alpha.1";
static const auto VERSION_ID = fmt::format(FMT_COMPILE("{} {}"), NAME, VERSION);

constexpr std::string_view USAGE = R"({0}.

Usage:
  curaengine_plugin_postprocess [--address <address>] [--port <port>]
  curaengine_plugin_postprocess (-h | --help)
  curaengine_plugin_postprocess --version

Options:
  -h --help                 Show this screen.
  --version                 Show version.
  -ip --address <address>   The IP address to connect the socket to [default: localhost].
  -p --port <port>          The port number to connect the socket to [default: 33701].
)";

} // namespace plugin::cmdline

#endif // PLUGIN_CMDLINE_H
