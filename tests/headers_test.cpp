#include "headers.h"
#include <gtest/gtest.h>

#include <string>
#include <vector>

TEST(iterHeaders, Empty) {
    std::vector<std::pair<std::string, std::string>> headers;
    iterHeaders("", [&](std::string_view name, std::string_view value) { headers.emplace_back(name, value); });
    EXPECT_TRUE(headers.empty());
}

TEST(iterHeaders, SkipRequestLine) {
    std::vector<std::pair<std::string, std::string>> headers;
    iterHeaders("GET / HTTP/1.1\r\n\r\n",
                [&](std::string_view name, std::string_view value) { headers.emplace_back(name, value); });
    EXPECT_TRUE(headers.empty());
}

TEST(iterHeaders, SingleHeader) {
    std::vector<std::pair<std::string, std::string>> headers;
    iterHeaders("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n",
                [&](std::string_view name, std::string_view value) { headers.emplace_back(name, value); });
    ASSERT_EQ(headers.size(), 1);
    EXPECT_EQ(headers[0].first, "Host");
    EXPECT_EQ(headers[0].second, "example.com");
}

TEST(iterHeaders, MultipleHeaders) {
    std::vector<std::pair<std::string, std::string>> headers;
    iterHeaders("GET / HTTP/1.1\r\nHost: example.com\r\nContent-Length: 100\r\n\r\n",
                [&](std::string_view name, std::string_view value) { headers.emplace_back(name, value); });
    ASSERT_EQ(headers.size(), 2);
    EXPECT_EQ(headers[0].first, "Host");
    EXPECT_EQ(headers[0].second, "example.com");
    EXPECT_EQ(headers[1].first, "Content-Length");
    EXPECT_EQ(headers[1].second, "100");
}

TEST(iterHeaders, MultipleSameHeaders) {
    std::vector<std::pair<std::string, std::string>> headers;
    iterHeaders("GET / HTTP/1.1\r\nAccept: text/html\r\nAccept: application/json\r\n\r\n",
                [&](std::string_view name, std::string_view value) { headers.emplace_back(name, value); });
    ASSERT_EQ(headers.size(), 2);
    EXPECT_EQ(headers[0].first, "Accept");
    EXPECT_EQ(headers[0].second, "text/html");
    EXPECT_EQ(headers[1].first, "Accept");
    EXPECT_EQ(headers[1].second, "application/json");
}

TEST(findHostPort, Simple) {
    auto [host, port] = findHostPort("GET / HTTP/1.1\r\nHost: example.com:8080\r\n\r\n");
    EXPECT_EQ(host, "example.com");
    EXPECT_EQ(port, "8080");
}

TEST(findHostPort, DefaultPort) {
    auto [host, port] = findHostPort("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n");
    EXPECT_EQ(host, "example.com");
    EXPECT_EQ(port, "80");
}

TEST(findHostPort, NoHost) {
    auto [host, port] = findHostPort("GET / HTTP/1.1\r\nAccept: */*\r\n\r\n");
    EXPECT_TRUE(host.empty());
    EXPECT_EQ(port, "80");
}

TEST(findContentLength, Simple) {
    auto result = findContentLength("HTTP/1.1 200 OK\r\nContent-Length: 4096\r\n\r\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 4096);
}

TEST(findContentLength, NoContentLength) {
    auto result = findContentLength("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
    EXPECT_FALSE(result.has_value());
}
