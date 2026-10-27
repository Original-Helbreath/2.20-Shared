#pragma once

#include "asio_config.h"
#include "frame_codec.h"
#include "message_codec.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// One framed TCP connection, the common half of hb::net::server and hb::net::client; program-neutral
// (no Login, World or game-client types). Every member runs on the
// connection's strand; send() and close() may be called from any thread and are posted there. Works the
// same whether the io_context runs on worker threads (servers) or is polled from a game loop (client).
namespace hb::net
{
	// Why a connection closed; on_close receives exactly one, once.
	enum class close_reason : std::uint8_t
	{
		local_close,     // this side called close()
		peer_closed,     // the peer ended the stream
		network_error,   // a socket read or write failed
		frame_error,     // the frame_decoder refused the stream (detail names the frame_error)
		protocol_error,  // a well-framed message the owner refused (unknown id, bad body, wrong state)
		idle_timeout,    // no whole frame received within the idle timeout
		deadline_expired,  // the fixed deadline (set_deadline) passed
		send_overflow,   // the peer stopped reading and the send queue passed its limit
		shutdown,        // the program is stopping
	};

	std::string_view to_string(close_reason reason);

	struct connection_options
	{
		std::uint32_t max_body_bytes = 65536;
		// Bytes waiting to be written before the peer is closed as send_overflow.
		std::size_t max_queued_bytes = 1024 * 1024;
	};

	class connection : public std::enable_shared_from_this<connection>
	{
	public:
		using frame_handler = std::function<void(connection&, const frame&)>;
		using close_handler = std::function<void(connection&, close_reason, std::string_view detail)>;

		// The socket's executor should be a strand (acceptor and connector make it one).
		static std::shared_ptr<connection> create(asio::ip::tcp::socket socket, const connection_options& options);

		connection(const connection&) = delete;
		connection& operator=(const connection&) = delete;
		~connection();

		// Starts the read loop. on_frame gets every complete frame in order; on_close is called once.
		void start(frame_handler on_frame, close_handler on_close);

		// Encodes the message under T::id and queues it. False only when it cannot be encoded.
		template <identified_message T>
		bool send(const T& message)
		{
			std::vector<std::byte> bytes;
			if (!encode(message, m_options.max_body_bytes, bytes))
				return false;
			send_encoded(std::move(bytes));
			return true;
		}

		// Queues one or more already encoded frames; written in the order queued.
		void send_encoded(std::vector<std::byte> bytes);

		// Closes now; queued frames are dropped.
		void close(close_reason reason, std::string_view detail = {});

		// Writes everything already queued, then closes (a refusal reply followed by a hang-up).
		void close_after_send(close_reason reason, std::string_view detail = {});

		// Closes with idle_timeout when no whole frame arrives for this long; reset only by a complete
		// frame, so a peer trickling bytes cannot hold the connection. Zero turns it off.
		void set_idle_timeout(std::chrono::milliseconds timeout);

		// Closes with deadline_expired this long from now whatever arrives (the time a peer has to log in
		// or register). Zero cancels it, e.g. once the peer has done so.
		void set_deadline(std::chrono::milliseconds from_now);

		bool is_open() const { return m_open.load(); }

		// Peer address as text (IPv4-mapped IPv6 shown as IPv4) and port; fixed at creation.
		const std::string& remote_address() const { return m_remote_address; }
		std::uint16_t remote_port() const { return m_remote_port; }
		const asio::ip::address& remote_ip() const { return m_remote_ip; }

		asio::any_io_executor executor() { return m_socket.get_executor(); }

	private:
		friend class server;
		friend class client;

		connection(asio::ip::tcp::socket socket, const connection_options& options);

		void read_more();
		void on_read(const asio::error_code& error, std::size_t size);
		void write_next();
		void arm_idle_timer();
		void finish(close_reason reason, std::string_view detail);

		asio::ip::tcp::socket m_socket;
		asio::steady_timer m_idle_timer;
		asio::steady_timer m_deadline_timer;
		connection_options m_options;
		frame_decoder m_decoder;
		std::array<std::byte, 4096> m_read_buffer{};
		std::deque<std::vector<std::byte>> m_send_queue;
		std::size_t m_queued_bytes = 0;
		std::chrono::milliseconds m_idle_timeout{ 0 };
		frame_handler m_on_frame;
		close_handler m_on_close;
		std::function<void()> m_release;  // set by the owning server or client; runs after on_close
		asio::ip::address m_remote_ip;
		std::string m_remote_address;
		std::uint16_t m_remote_port = 0;
		std::atomic<bool> m_open{ true };  // read by is_open() from any thread
		bool m_writing = false;
		bool m_closing_after_send = false;
		close_reason m_pending_reason = close_reason::local_close;
		std::string m_pending_detail;
	};
}
