#pragma once

#include "asio_config.h"
#include "connection.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

// The listening role: binds a port, accepts connections and owns them until they close. A program that
// takes connections makes one hb::net::server per port (the Login server: one for game clients, one for
// the World link; the World server: one for game clients). The connecting role is hb::net::client.
namespace hb::net
{
	struct server_options
	{
		std::string bind_address = "0.0.0.0";
		std::uint16_t port = 0;  // 0 picks a free port (tests); local_port() reports it
		connection_options connection;
		// Open connections held at most; a connection accepted beyond it is closed at once, unread.
		std::size_t max_connections = 1000;
	};

	class server : public std::enable_shared_from_this<server>
	{
	public:
		// Called for every accepted connection, on that connection's strand, before it reads; the
		// handler calls connection::start() with its frame and close handlers (or close() to refuse it).
		using accept_handler = std::function<void(const std::shared_ptr<connection>&)>;

		static std::shared_ptr<server> create(asio::io_context& context);

		server(const server&) = delete;
		server& operator=(const server&) = delete;

		// Binds and starts accepting. False, with error set, when the address or port cannot be bound.
		bool listen(const server_options& options, accept_handler on_accept, std::string& error);

		std::uint16_t local_port() const { return m_local_port; }

		// Stops accepting and closes every owned connection with close_reason::shutdown.
		void stop();

		std::size_t connection_count() const;

	private:
		explicit server(asio::io_context& context);

		void accept_next();
		void release(const std::shared_ptr<connection>& released);

		asio::io_context& m_context;
		asio::ip::tcp::acceptor m_acceptor;
		asio::steady_timer m_retry_timer;  // paces accept after an error (e.g. out of descriptors)
		server_options m_options;
		accept_handler m_on_accept;
		std::uint16_t m_local_port = 0;
		bool m_stopped = false;
		mutable std::mutex m_mutex;  // guards m_connections: connections close on their own strands
		std::unordered_set<std::shared_ptr<connection>> m_connections;
	};
}
