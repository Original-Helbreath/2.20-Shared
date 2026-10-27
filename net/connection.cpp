#include "connection.h"

#include <span>
#include <utility>

namespace hb::net
{
	std::string_view to_string(close_reason reason)
	{
		switch (reason)
		{
		case close_reason::local_close:
			return "local close";
		case close_reason::peer_closed:
			return "peer closed";
		case close_reason::network_error:
			return "network error";
		case close_reason::frame_error:
			return "frame error";
		case close_reason::protocol_error:
			return "protocol error";
		case close_reason::idle_timeout:
			return "idle timeout";
		case close_reason::deadline_expired:
			return "deadline expired";
		case close_reason::send_overflow:
			return "send overflow";
		case close_reason::shutdown:
			return "shutdown";
		}
		return "unknown";
	}

	std::shared_ptr<connection> connection::create(asio::ip::tcp::socket socket, const connection_options& options)
	{
		return std::shared_ptr<connection>(new connection(std::move(socket), options));
	}

	connection::connection(asio::ip::tcp::socket socket, const connection_options& options)
		: m_socket(std::move(socket))
		, m_idle_timer(m_socket.get_executor())
		, m_deadline_timer(m_socket.get_executor())
		, m_options(options)
		, m_decoder(options.max_body_bytes)
	{
		asio::error_code error;
		const asio::ip::tcp::endpoint remote = m_socket.remote_endpoint(error);
		if (!error)
		{
			m_remote_ip = remote.address();
			if (m_remote_ip.is_v6() && m_remote_ip.to_v6().is_v4_mapped())
				m_remote_ip = asio::ip::make_address_v4(asio::ip::v4_mapped, m_remote_ip.to_v6());
			m_remote_address = m_remote_ip.to_string();
			m_remote_port = remote.port();
		}
	}

	connection::~connection() = default;

	void connection::start(frame_handler on_frame, close_handler on_close)
	{
		asio::post(m_socket.get_executor(),
			[self = shared_from_this(), on_frame = std::move(on_frame), on_close = std::move(on_close)]() mutable {
				self->m_on_frame = std::move(on_frame);
				self->m_on_close = std::move(on_close);
				if (!self->m_open)
					return;
				self->arm_idle_timer();
				self->read_more();
			});
	}

	void connection::send_encoded(std::vector<std::byte> bytes)
	{
		asio::post(m_socket.get_executor(), [self = shared_from_this(), bytes = std::move(bytes)]() mutable {
			if (!self->m_open || self->m_closing_after_send || bytes.empty())
				return;
			self->m_queued_bytes += bytes.size();
			if (self->m_queued_bytes > self->m_options.max_queued_bytes)
			{
				self->finish(close_reason::send_overflow, "the peer is not reading");
				return;
			}
			self->m_send_queue.push_back(std::move(bytes));
			if (!self->m_writing)
				self->write_next();
		});
	}

	void connection::close(close_reason reason, std::string_view detail)
	{
		asio::post(m_socket.get_executor(), [self = shared_from_this(), reason, detail = std::string(detail)] {
			self->finish(reason, detail);
		});
	}

	void connection::close_after_send(close_reason reason, std::string_view detail)
	{
		asio::post(m_socket.get_executor(), [self = shared_from_this(), reason, detail = std::string(detail)] {
			if (!self->m_open || self->m_closing_after_send)
				return;
			self->m_closing_after_send = true;
			self->m_pending_reason = reason;
			self->m_pending_detail = detail;
			if (!self->m_writing)
				self->finish(reason, detail);
		});
	}

	void connection::set_idle_timeout(std::chrono::milliseconds timeout)
	{
		asio::post(m_socket.get_executor(), [self = shared_from_this(), timeout] {
			self->m_idle_timeout = timeout;
			if (self->m_open)
				self->arm_idle_timer();
		});
	}

	void connection::set_deadline(std::chrono::milliseconds from_now)
	{
		asio::post(m_socket.get_executor(), [self = shared_from_this(), from_now] {
			if (!self->m_open)
				return;
			if (from_now.count() <= 0)
			{
				self->m_deadline_timer.cancel();
				return;
			}
			self->m_deadline_timer.expires_after(from_now);
			self->m_deadline_timer.async_wait([self](const asio::error_code& error) {
				if (error || !self->m_open)
					return;
				if (self->m_deadline_timer.expiry() > asio::steady_timer::clock_type::now())
					return;  // moved after this wait was queued
				self->finish(close_reason::deadline_expired, {});
			});
		});
	}

	void connection::read_more()
	{
		m_socket.async_read_some(asio::buffer(m_read_buffer),
			[self = shared_from_this()](const asio::error_code& error, std::size_t size) { self->on_read(error, size); });
	}

	void connection::on_read(const asio::error_code& error, std::size_t size)
	{
		if (!m_open)
			return;
		if (error)
		{
			if (error == asio::error::eof || error == asio::error::connection_reset)
				finish(close_reason::peer_closed, error.message());
			else if (error != asio::error::operation_aborted)
				finish(close_reason::network_error, error.message());
			return;
		}
		// Contract: check append's result, then drain next() until it stops returning frame_ready.
		if (!m_decoder.append(std::span<const std::byte>(m_read_buffer.data(), size)))
		{
			finish(close_reason::frame_error, hb::net::to_string(m_decoder.error()));
			return;
		}
		frame received;
		while (m_open && !m_closing_after_send)
		{
			const decode_status status = m_decoder.next(received);
			if (status == decode_status::need_more)
				break;
			if (status == decode_status::failed)
			{
				finish(close_reason::frame_error, hb::net::to_string(m_decoder.error()));
				return;
			}
			arm_idle_timer();  // only a whole frame counts as activity
			if (m_on_frame)
				m_on_frame(*this, received);
		}
		if (m_open && !m_closing_after_send)
			read_more();
	}

	void connection::write_next()
	{
		if (m_send_queue.empty())
		{
			m_writing = false;
			if (m_closing_after_send)
				finish(m_pending_reason, m_pending_detail);
			return;
		}
		m_writing = true;
		asio::async_write(m_socket, asio::buffer(m_send_queue.front()),
			[self = shared_from_this()](const asio::error_code& error, std::size_t) {
				if (!self->m_open)
				{
					self->m_writing = false;
					self->m_send_queue.clear();
					self->m_queued_bytes = 0;
					return;
				}
				if (error)
				{
					self->m_writing = false;
					self->m_send_queue.clear();
					self->m_queued_bytes = 0;
					if (error != asio::error::operation_aborted)
						self->finish(close_reason::network_error, error.message());
					return;
				}
				self->m_queued_bytes -= self->m_send_queue.front().size();
				self->m_send_queue.pop_front();
				self->write_next();
			});
	}

	void connection::arm_idle_timer()
	{
		if (m_idle_timeout.count() <= 0)
		{
			m_idle_timer.cancel();
			return;
		}
		m_idle_timer.expires_after(m_idle_timeout);
		m_idle_timer.async_wait([self = shared_from_this()](const asio::error_code& error) {
			if (error || !self->m_open)
				return;
			if (self->m_idle_timer.expiry() > asio::steady_timer::clock_type::now())
				return;  // re-armed after this wait was queued
			self->finish(close_reason::idle_timeout, {});
		});
	}

	void connection::finish(close_reason reason, std::string_view detail)
	{
		if (!m_open)
			return;
		m_open = false;
		m_idle_timer.cancel();
		m_deadline_timer.cancel();
		asio::error_code ignored;
		m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
		m_socket.close(ignored);
		// A write in flight still refers to the front buffer; its completion empties the queue instead.
		if (!m_writing)
		{
			m_send_queue.clear();
			m_queued_bytes = 0;
		}
		// Moved out first: the handlers may hold the last owner of this connection.
		frame_handler on_frame = std::move(m_on_frame);
		close_handler on_close = std::move(m_on_close);
		std::function<void()> release = std::move(m_release);
		m_on_frame = nullptr;
		if (on_close)
			on_close(*this, reason, detail);
		if (release)
			release();
	}
}
