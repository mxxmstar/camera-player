#include "server/http/server.hpp"
#include "rtsp/rtspmgr.h"
#include "log/logger.h"

#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#endif

static std::unique_ptr<http::Server> g_http_server;
static std::string g_video_file;

static std::string get_exe_dir()
{
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::filesystem::path p(path);
    return p.parent_path().string();
#else
    return std::filesystem::current_path().string();
#endif
}

static std::string resolve_video_path(const std::string& filename)
{
    if (std::filesystem::exists(filename)) {
        LOG_INFO("[Main] Found video at cwd: {}", std::filesystem::absolute(filename).string());
        return filename;
    }

    std::string exe_dir = get_exe_dir();
    std::string exe_path = exe_dir + "\\" + filename;
    if (std::filesystem::exists(exe_path)) {
        LOG_INFO("[Main] Found video at exe dir: {}", exe_path);
        return exe_path;
    }

    std::string parent1 = std::filesystem::path(exe_dir).parent_path().string() + "\\" + filename;
    if (std::filesystem::exists(parent1)) {
        LOG_INFO("[Main] Found video at parent1: {}", parent1);
        return parent1;
    }

    std::string parent2 = std::filesystem::path(exe_dir).parent_path().parent_path().string() + "\\" + filename;
    if (std::filesystem::exists(parent2)) {
        LOG_INFO("[Main] Found video at parent2: {}", parent2);
        return parent2;
    }

    std::string parent3 = std::filesystem::path(exe_dir).parent_path().parent_path().parent_path().string() + "\\" + filename;
    if (std::filesystem::exists(parent3)) {
        LOG_INFO("[Main] Found video at parent3: {}", parent3);
        return parent3;
    }

    LOG_WARN("[Main] Video not found. Searched:\n"
             "  1. cwd: {}\n"
             "  2. exe: {}\n"
             "  3. p1:  {}\n"
             "  4. p2:  {}\n"
             "  5. p3:  {}",
             std::filesystem::absolute(filename).string(),
             exe_path, parent1, parent2, parent3);

    return filename;
}

static void signal_handler(int /*signum*/)
{
    LOG_INFO("[Main] Caught signal, shutting down...");
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

    ai_camera::log::Init("ai-camera", "");

    LOG_INFO("[Main] Starting RTSP server on rtsp://localhost:8554");
    auto& rtsp_mgr = rtsp::RtspManager::Instance();

    if (!g_video_file.empty()) {
        g_video_file = resolve_video_path(g_video_file);
        rtsp_mgr.SetVideoFile(g_video_file);
        LOG_INFO("[Main] Video file: {}", g_video_file);
    }

    rtsp_mgr.Start(8554);

    g_http_server = std::make_unique<http::Server>("0.0.0.0", 8080);

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

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    LOG_INFO("[Main] Starting HTTP server on http://localhost:8080");
    g_http_server->run();

    LOG_INFO("[Main] Server stopped. Goodbye!");
    ai_camera::log::Shutdown();
    return 0;
}