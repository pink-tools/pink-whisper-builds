#include "whisper.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#define CLOSE_SOCKET closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
typedef int socket_t;
#define CLOSE_SOCKET close
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

struct whisper_context* g_ctx = nullptr;

bool recv_all(socket_t sock, void* buf, size_t len) {
    char* ptr = (char*)buf;
    size_t remaining = len;
    while (remaining > 0) {
        int n = recv(sock, ptr, remaining, 0);
        if (n <= 0) return false;
        ptr += n;
        remaining -= n;
    }
    return true;
}

bool send_all(socket_t sock, const void* buf, size_t len) {
    const char* ptr = (const char*)buf;
    size_t remaining = len;
    while (remaining > 0) {
        int n = send(sock, ptr, remaining, 0);
        if (n <= 0) return false;
        ptr += n;
        remaining -= n;
    }
    return true;
}

std::string transcribe(const std::vector<float>& pcmf32) {
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress = false;
    wparams.print_special = false;
    wparams.print_realtime = false;
    wparams.print_timestamps = false;
    wparams.single_segment = false;
    wparams.language = "auto";
    wparams.n_threads = std::min(4, (int)std::thread::hardware_concurrency());

    if (whisper_full(g_ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
        return "[error: inference failed]";
    }

    std::string result;
    int n_segments = whisper_full_n_segments(g_ctx);
    for (int i = 0; i < n_segments; i++) {
        const char* text = whisper_full_get_segment_text(g_ctx, i);
        result += text;
    }
    return result;
}

void handle_client(socket_t client) {
    // Read audio size (4 bytes, little-endian)
    uint32_t audio_size;
    if (!recv_all(client, &audio_size, 4)) {
        CLOSE_SOCKET(client);
        return;
    }

    // Read audio data (16-bit PCM, 16kHz mono)
    std::vector<int16_t> pcm16(audio_size / 2);
    if (!recv_all(client, pcm16.data(), audio_size)) {
        CLOSE_SOCKET(client);
        return;
    }

    // Convert to float32
    std::vector<float> pcmf32(pcm16.size());
    for (size_t i = 0; i < pcm16.size(); i++) {
        pcmf32[i] = (float)pcm16[i] / 32768.0f;
    }

    // Transcribe
    std::string text = transcribe(pcmf32);

    // Send response
    uint32_t text_size = text.size();
    send_all(client, &text_size, 4);
    send_all(client, text.data(), text_size);

    CLOSE_SOCKET(client);
}

int main(int argc, char** argv) {
    const char* model_path = "ggml-large-v3.bin";
    int port = 7465;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            model_path = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        }
    }

    fprintf(stderr, "pink-whisper: loading model %s\n", model_path);

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = true;
    cparams.flash_attn = true;

    g_ctx = whisper_init_from_file_with_params(model_path, cparams);
    if (!g_ctx) {
        fprintf(stderr, "pink-whisper: failed to load model\n");
        return 1;
    }

    fprintf(stderr, "pink-whisper: model loaded\n");

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
        fprintf(stderr, "pink-whisper: socket failed\n");
        return 1;
    }

    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "pink-whisper: bind failed\n");
        return 1;
    }

    if (listen(server, 5) == SOCKET_ERROR) {
        fprintf(stderr, "pink-whisper: listen failed\n");
        return 1;
    }

    fprintf(stderr, "pink-whisper: listening on port %d\n", port);

    while (true) {
        socket_t client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        handle_client(client);
    }

    whisper_free(g_ctx);
    CLOSE_SOCKET(server);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
