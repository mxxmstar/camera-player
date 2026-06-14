#include "server/http/server.hpp"

#include <csignal>
#include <iostream>
#include <memory>

static std::unique_ptr<http::Server> g_server;

static void signal_handler(int /*signum*/)
{
    std::cout << "\n[Main] Caught signal, shutting down..." << std::endl;
    if (g_server)
        g_server->stop();
}

int main()
{
    // ------------------------------------------------------------
    // 1. Create server on port 8080
    // ------------------------------------------------------------
    g_server = std::make_unique<http::Server>("0.0.0.0", 8080);

    // ------------------------------------------------------------
    // 2. Register routes
    // ------------------------------------------------------------
    auto &router = g_server->router();

    // GET /
    router.get("/",
        [](const http::Request & /*req*/) -> http::Response
        {
            return http::Response::ok(
                "<html>"
                "<head><title>ai-camera</title></head>"
                "<body>"
                "<h1>Welcome to ai-camera!</h1>"
                "<p>Try these endpoints:</p>"
                "<ul>"
                "<li><a href='/hello'>/hello</a></li>"
                "<li><a href='/json'>/json</a></li>"
                "<li><a href='/echo'>/echo (POST)</a></li>"
                "</ul>"
                "</body>"
                "</html>",
                "text/html");
        });

    // GET /hello
    router.get("/hello",
        [](const http::Request & /*req*/) -> http::Response
        {
            return http::Response::ok("Hello, ASIO HTTP Server!\n");
        });

    // GET /json
    router.get("/json",
        [](const http::Request & /*req*/) -> http::Response
        {
            return http::Response::ok(
                "{ \"message\": \"Hello from ai-camera\", \"status\": \"ok\" }\n",
                "application/json");
        });

    // POST /echo — echoes back the body
    router.post("/echo",
        [](const http::Request &req) -> http::Response
        {
            return http::Response::ok(req.body);
        });

    // ------------------------------------------------------------
    // 3. Register signal handlers for graceful shutdown
    // ------------------------------------------------------------
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // ------------------------------------------------------------
    // 4. Start the server (blocks)
    // ------------------------------------------------------------
    std::cout << "[Main] Starting HTTP server on http://localhost:8080" << std::endl;
    g_server->run();

    std::cout << "[Main] Server stopped. Goodbye!" << std::endl;
    return 0;
}