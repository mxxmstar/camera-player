#include "server/http/server.hpp"
#include "rtsp/rtspmgr.h"
#include "log/logger.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>

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
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::filesystem::path exe_path(path);
    return exe_path.parent_path().string();
#else
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        std::filesystem::path exe_path(path);
        return exe_path.parent_path().string();
    }
    return "";
#endif
}

static std::string resolve_video_path(const std::string& filename)
{
    std::filesystem::path p(filename);
    if (p.is_absolute()) {
        return filename;
    }
    std::string exe_dir = get_exe_dir();
    if (!exe_dir.empty()) {
        std::filesystem::path candidate = std::filesystem::path(exe_dir) / filename;
        if (std::filesystem::exists(candidate)) {
            return candidate.string();
        }
    }
    if (std::filesystem::exists(p)) {
        return std::filesystem::absolute(p).string();
    }
    return filename;
}

static void print_usage(const char* prog)
{
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --video <file>   Set video file path (default: test.h264)\n"
              << "  --help           Show this help message\n";
}

static void signal_handler(int sig)
{
    (void)sig;
    if (g_http_server) {
        g_http_server->stop();
    }
}

static int run_opengl_test()
{
    std::cout << "========== OpenGL Test ==========" << std::endl;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "OpenGL Test", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    std::cout << "OpenGL Vendor:   " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "OpenGL Version:  " << glGetString(GL_VERSION) << std::endl;

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "========== OpenGL Test End ==========" << std::endl;
    return 0;
}

static int run_ffmpeg_test()
{
    std::cout << "\n========== FFmpeg Test ==========" << std::endl;
    std::cout << "FFmpeg version: " << av_version_info() << std::endl;

    std::cout << "\n----- Video Codecs -----" << std::endl;
    const AVCodec* codec = nullptr;
    void* iter = nullptr;
    int video_count = 0;
    while ((codec = av_codec_iterate(&iter)) != nullptr) {
        if (av_codec_is_decoder(codec) && codec->type == AVMEDIA_TYPE_VIDEO) {
            std::cout << "  [V] " << codec->name;
            if (codec->long_name)
                std::cout << " - " << codec->long_name;
            std::cout << std::endl;
            if (++video_count >= 20)
                break;
        }
    }

    std::cout << "\n----- Audio Codecs -----" << std::endl;
    iter = nullptr;
    int audio_count = 0;
    while ((codec = av_codec_iterate(&iter)) != nullptr) {
        if (av_codec_is_decoder(codec) && codec->type == AVMEDIA_TYPE_AUDIO) {
            std::cout << "  [A] " << codec->name;
            if (codec->long_name)
                std::cout << " - " << codec->long_name;
            std::cout << std::endl;
            if (++audio_count >= 20)
                break;
        }
    }

    std::cout << "\n----- Video Formats (Containers) -----" << std::endl;
    const AVInputFormat* fmt = nullptr;
    void* fmt_iter = nullptr;
    int fmt_count = 0;
    while ((fmt = av_demuxer_iterate(&fmt_iter)) != nullptr) {
        std::cout << "  [F] " << fmt->name;
        if (fmt->long_name)
            std::cout << " - " << fmt->long_name;
        std::cout << std::endl;
        if (++fmt_count >= 20)
            break;
    }
    if (fmt_count >= 20)
        std::cout << "  ... (and more)" << std::endl;

    std::cout << "\n========== FFmpeg Test End ==========\n" << std::endl;
    return 0;
}

class CounterWidget : public QWidget
{
    Q_OBJECT
public:
    CounterWidget(QWidget* parent = nullptr) : QWidget(parent), m_count(0)
    {
        auto* layout = new QVBoxLayout(this);

        m_label = new QLabel("Count: 0", this);
        m_label->setAlignment(Qt::AlignCenter);
        QFont font = m_label->font();
        font.setPointSize(24);
        m_label->setFont(font);

        m_button = new QPushButton("Click Me!", this);
        m_button->setMinimumHeight(50);

        connect(m_button, &QPushButton::clicked, this, &CounterWidget::onClick);

        layout->addWidget(m_label);
        layout->addWidget(m_button);
        setLayout(layout);

        setWindowTitle("Qt6 Counter Test");
        resize(300, 150);
    }

private slots:
    void onClick()
    {
        ++m_count;
        m_label->setText(QString("Count: %1").arg(m_count));
    }

private:
    QLabel* m_label;
    QPushButton* m_button;
    int m_count;
};

static int run_qt_test(int argc, char* argv[])
{
    std::cout << "\n========== Qt6 Widgets Test ==========" << std::endl;

    QApplication app(argc, argv);

    std::cout << "Qt Compile Version: " << QT_VERSION_STR << std::endl;
    std::cout << "Qt Runtime Version: " << qVersion() << std::endl;

    CounterWidget widget;
    widget.show();

    std::cout << "Qt Widgets window shown. Close window to continue..." << std::endl;
    int ret = app.exec();

    std::cout << "========== Qt6 Widgets Test End ==========\n" << std::endl;
    return ret;
}

int main(int argc, char* argv[])
{
    run_ffmpeg_test();
    run_qt_test(argc, argv);
    // return run_opengl_test();
    return 0;
}

#include "main.moc"