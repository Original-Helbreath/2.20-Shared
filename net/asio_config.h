#pragma once

// The one place shared\net includes Asio (shared\third_party\asio, 1.36.0, standalone, header-only).
// Consumers add shared\third_party\asio\include to the include path; the macros below are set here so
// every translation unit sees Asio configured the same way.
#if !defined(ASIO_STANDALONE)
#define ASIO_STANDALONE
#endif
#if !defined(ASIO_NO_DEPRECATED)
#define ASIO_NO_DEPRECATED
#endif
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0A00  // Windows 10; tells Asio the target instead of letting it guess
#endif

#include <asio/connect.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>
#include <asio/strand.hpp>
#include <asio/write.hpp>
