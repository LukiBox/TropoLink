#pragma once

// Optional local AI commentary: a single blocking HTTP POST to an Ollama instance on
// the loopback interface. Loopback-only by construction — the host is hard-coded to
// 127.0.0.1 and never configurable to a remote address. This translation unit is not
// compiled in the Air-Gap flavor (see core/CMakeLists.txt).

#include "core/common/expected.h"

#include <string>

namespace tl::ai {

struct OllamaRequest {
    int port = 11434;                 // loopback port only; host is always 127.0.0.1
    std::string model = "llama3.2";
    std::string prompt;
    int timeoutSeconds = 60;
};

// Returns the generated commentary paragraph, or an error (Ollama absent, timeout...).
[[nodiscard]] Expected<std::string> generateCommentary(const OllamaRequest& request);

// True when an Ollama server answers on the loopback port.
[[nodiscard]] bool ollamaAvailable(int port = 11434);

} // namespace tl::ai
