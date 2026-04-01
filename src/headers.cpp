#include "headers.h"

#include <algorithm>
#include <charconv>
#include <ranges>
#include <string_view>

using namespace std::string_view_literals;

constexpr auto line_delimiter = "\r\n"sv;
constexpr auto header_separator = ": "sv;
constexpr auto default_http_port = "80";
constexpr size_t max_content_length = 10 * 1024 * 1024;

bool icaseEqual(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char lhs, char rhs) {
        return std::tolower(static_cast<unsigned char>(lhs)) == std::tolower(static_cast<unsigned char>(rhs));
    });
}

void iterHeaders(std::string_view req, Callback &&callback) {
    bool first_line = true;
    for (auto line_range : req | std::views::split(line_delimiter)) {
        std::string_view line(line_range.begin(), line_range.end());
        if (first_line) {
            first_line = false;
            continue;
        }
        if (line.empty()) {
            break;
        }
        auto sep_pos = line.find(header_separator);
        if (sep_pos == std::string_view::npos) {
            continue;
        }
        auto name = line.substr(0, sep_pos);
        auto value = line.substr(sep_pos + header_separator.size());
        callback(name, value);
    }
}

std::pair<std::string, std::string> findHostPort(std::string_view req) {
    std::string host;
    std::string port(default_http_port);
    iterHeaders(req, [&](std::string_view name, std::string_view value) {
        if (icaseEqual(name, "Host")) {
            auto colon_pos = value.find(':');
            if (colon_pos != std::string_view::npos) {
                host = value.substr(0, colon_pos);
                port = value.substr(colon_pos + 1);
            } else {
                host = value;
            }
        }
    });
    return {host, port};
}

std::optional<size_t> findContentLength(std::string_view rsp) {
    std::optional<size_t> result;
    iterHeaders(rsp, [&](std::string_view name, std::string_view value) {
        if (icaseEqual(name, "Content-Length")) {
            size_t length = 0;
            auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), length);
            if (ec == std::errc{} && length <= max_content_length) {
                result = length;
            }
        }
    });
    return result;
}
