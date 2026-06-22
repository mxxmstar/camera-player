#include "server/http/server.hpp"
#include "rtsp/rtspmgr.h"

#include <csignal>
#include <iostream>
#include <memory>
#include <string>

static std::unique_ptr<http::Server> g_http_server;
static std::string g_video_file;

static void signal_handler(int /*signum*/)
{
    std::cout << "\n[Main] Caught signal, shutting down..." << std::endl;
    if (g_http_server)
        g_http_server->stop();
    rtsp::RtspManager::Instance().Stop();
}

static void print_usage(const char* prog)
{
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --video <file>   Raw H.264 video file to stream via RTSP\n"
              << "  --help           Show this help\n"
              << std::endl;
}

int main(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--video" && i + 1 < argc) {
            g_video_file = argv[++i];
        } else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }
    if (g_video_file.empty()) {
        g_video_file = "test.h264";
    }
    // ------------------------------------------------------------
    // 1. Start RTSP server on port 8554
    // ------------------------------------------------------------
    std::cout << "[Main] Starting RTSP server on rtsp://localhost:8554" << std::endl;
    auto& rtsp_mgr = rtsp::RtspManager::Instance();

    if (!g_video_file.empty()) {
        rtsp_mgr.SetVideoFile(g_video_file);
        std::cout << "[Main] Video file: " << g_video_file << std::endl;
    }

    rtsp_mgr.Start(8554);

    // ------------------------------------------------------------
    // 2. Create HTTP server on port 8080
    // ------------------------------------------------------------
    g_http_server = std::make_unique<http::Server>("0.0.0.0", 8080);

    // ------------------------------------------------------------
    // 3. Register routes
    // ------------------------------------------------------------
    auto &router = g_http_server->router();

    router.get("/",
        [](const http::Request & /*req*/) -> http::Response
        {
            return http::Response::ok(
                "<html>"
                "<head><title>ai-camera</title></head>"
                "<body>"
                "<h1>Welcome to ai-camera!</h1>"
                "<p>RTSP stream available at: <b>rtsp://localhost:8554/live</b></p>"
                "</body>"
                "</html>",
                "text/html");
        });

    router.get("/hello",
        [](const http::Request & /*req*/) -> http::Response
        {
            return http::Response::ok("Hello, ASIO HTTP Server!\n");
        });

    router.get("/json",
        [](const http::Request & /*req*/) -> http::Response
        {
            return http::Response::ok(
                "{ \"message\": \"Hello from ai-camera\", \"status\": \"ok\" }\n",
                "application/json");
        });

    router.post("/echo",
        [](const http::Request &req) -> http::Response
        {
            return http::Response::ok(req.body);
        });

    // ------------------------------------------------------------
    // 4. Register signal handlers for graceful shutdown
    // ------------------------------------------------------------
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // ------------------------------------------------------------
    // 5. Start the HTTP server (blocks)
    // ------------------------------------------------------------
    std::cout << "[Main] Starting HTTP server on http://localhost:8080" << std::endl;
    g_http_server->run();

    std::cout << "[Main] Server stopped. Goodbye!" << std::endl;
    return 0;
}