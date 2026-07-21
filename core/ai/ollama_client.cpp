#include "core/ai/ollama_client.h"

#include <nlohmann/json.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
constexpr int INVALID_SOCKET = -1;
#endif

#include <cstring>
#include <sstream>

namespace tl::ai {

namespace {

class Socket {
  public:
    Socket() {
#ifdef _WIN32
        WSADATA data;
        wsaOk_ = WSAStartup(MAKEWORD(2, 2), &data) == 0;
#endif
    }
    ~Socket() {
        close();
#ifdef _WIN32
        if (wsaOk_) {
            WSACleanup();
        }
#endif
    }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    [[nodiscard]] bool connectLoopback(int port, int timeoutSeconds) {
#ifdef _WIN32
        if (!wsaOk_) {
            return false;
        }
#endif
        handle_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (handle_ == INVALID_SOCKET) {
            return false;
        }
#ifdef _WIN32
        const DWORD timeoutMs = static_cast<DWORD>(timeoutSeconds) * 1000;
        setsockopt(handle_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs),
                   sizeof(timeoutMs));
        setsockopt(handle_, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeoutMs),
                   sizeof(timeoutMs));
#else
        timeval tv{timeoutSeconds, 0};
        setsockopt(handle_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(handle_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<unsigned short>(port));
        // Loopback only — never a remote host.
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        return ::connect(handle_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
    }

    [[nodiscard]] bool sendAll(const std::string& data) const {
        std::size_t sent = 0;
        while (sent < data.size()) {
            const auto n = ::send(handle_, data.data() + sent, static_cast<int>(data.size() - sent), 0);
            if (n <= 0) {
                return false;
            }
            sent += static_cast<std::size_t>(n);
        }
        return true;
    }

    [[nodiscard]] std::string receiveAll() const {
        std::string out;
        char buf[8192];
        for (;;) {
            const auto n = ::recv(handle_, buf, sizeof(buf), 0);
            if (n <= 0) {
                break;
            }
            out.append(buf, static_cast<std::size_t>(n));
        }
        return out;
    }

    void close() {
        if (handle_ != INVALID_SOCKET) {
#ifdef _WIN32
            ::closesocket(handle_);
#else
            ::close(handle_);
#endif
            handle_ = INVALID_SOCKET;
        }
    }

  private:
    SocketHandle handle_ = INVALID_SOCKET;
#ifdef _WIN32
    bool wsaOk_ = false;
#endif
};

} // namespace

bool ollamaAvailable(int port) {
    Socket socket;
    return socket.connectLoopback(port, 2);
}

Expected<std::string> generateCommentary(const OllamaRequest& request) {
    nlohmann::json body{{"model", request.model}, {"prompt", request.prompt}, {"stream", false}};
    const std::string payload = body.dump();

    std::ostringstream http;
    http << "POST /api/generate HTTP/1.1\r\n"
         << "Host: 127.0.0.1:" << request.port << "\r\n"
         << "Content-Type: application/json\r\n"
         << "Content-Length: " << payload.size() << "\r\n"
         << "Connection: close\r\n\r\n"
         << payload;

    Socket socket;
    if (!socket.connectLoopback(request.port, request.timeoutSeconds)) {
        return Error{"Ollama is not reachable on 127.0.0.1:" + std::to_string(request.port)};
    }
    if (!socket.sendAll(http.str())) {
        return Error{"failed to send the request to Ollama"};
    }
    const std::string response = socket.receiveAll();
    const auto headerEnd = response.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return Error{"malformed HTTP response from Ollama"};
    }
    std::string bodyPart = response.substr(headerEnd + 4);
    // Tolerate chunked transfer encoding by scanning for the JSON object bounds.
    const auto firstBrace = bodyPart.find('{');
    const auto lastBrace = bodyPart.rfind('}');
    if (firstBrace == std::string::npos || lastBrace == std::string::npos || lastBrace < firstBrace) {
        return Error{"no JSON payload in the Ollama response"};
    }
    nlohmann::json doc =
        nlohmann::json::parse(bodyPart.substr(firstBrace, lastBrace - firstBrace + 1), nullptr, false);
    if (doc.is_discarded() || !doc.contains("response")) {
        return Error{"unexpected Ollama response format"};
    }
    return doc["response"].get<std::string>();
}

} // namespace tl::ai
