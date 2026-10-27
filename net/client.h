#pragma once

#include "asio_config.h"
#include "connection.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

// The connecting role: opens one outgoing connection and, if asked, reconnects after a failure or a
// close. The game client is a client of the Login server, then of the World server; the World server is
// a client of the Login link. The listening role is hb::net::server.
//
// No threads of its own: the game client may drive it by polling its io_context from the game loop
// (io_context::poll), the servers by running theirs on worker threads.
namespace hb::net
{
	struct client_options
	{
		std::string host;  // name or address text, resolved on every attempt
		std::uint16_t port = 0;
		connection_options connection;
		bool reconnects = false;  // retry after a failed attempt or a closed connection
		std::chrono::milliseconds retry_delay{ 2000 };
	};

	class client : public std::enable_shared_from_this<client>
	{
	public:
		// Called on the new connection's strand before it reads; the handler calls connection::start().
		using connect_handler = std::function<void(const std::shared_ptr<connection>&)>;
		// Called when an attempt fails (resolve or connect); a retry follows if reconnects is set.
		using failure_handler = std::function<void(std::string_view error)>;

		static std::shared_ptr<client> create(asio::io_context& context, const client_options& options);

		client(const client&) = delete;
		client& operator=(const client&) = delete;

		void start(connect_handler on_connect, failure_handler on_failure);

		// No more attempts; the current connection, if any, closes with close_reason::shutdown.
		void stop();

		// The open connection, or null while connecting or waiting to retry.
		std::shared_ptr<connection> current() const;

	private:
		client(asio::io_context& context, const client_options& options);

		void attempt();
		void schedule_retry();
		void released();

		asio::io_context& m_context;
		asio::strand<asio::io_context::executor_type> m_strand;
		asio::ip::tcp::resolver m_resolver;
		asio::steady_timer m_retry_timer;
		client_options m_options;
		connect_handler m_on_connect;
		failure_handler m_on_failure;
		bool m_stopped = false;
		mutable std::mutex m_mutex;  // guards m_current, read by current() from any thread
		std::shared_ptr<connection> m_current;
	};
}
