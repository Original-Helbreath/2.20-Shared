#include "server.h"

#include "log.h"

#include <chrono>
#include <format>
#include <utility>
#include <vector>

namespace hb::net
{
	namespace
	{
		constexpr std::chrono::milliseconds accept_retry_delay{ 100 };
	}

	std::shared_ptr<server> server::create(asio::io_context& context)
	{
		return std::shared_ptr<server>(new server(context));
	}

	server::server(asio::io_context& context)
		: m_context(context)
		, m_acceptor(asio::make_strand(context))
		, m_retry_timer(m_acceptor.get_executor())
	{
	}

	bool server::listen(const server_options& options, accept_handler on_accept, std::string& error)
	{
		m_options = options;
		m_on_accept = std::move(on_accept);
		asio::error_code code;
		const asio::ip::address address = asio::ip::make_address(options.bind_address, code);
		if (code)
		{
			error = std::format("bad bind address '{}': {}", options.bind_address, code.message());
			return false;
		}
		const asio::ip::tcp::endpoint endpoint(address, options.port);
		if (m_acceptor.open(endpoint.protocol(), code); !code)
			if (m_acceptor.set_option(asio::socket_base::reuse_address(true), code); !code)
				if (m_acceptor.bind(endpoint, code); !code)
					m_acceptor.listen(asio::socket_base::max_listen_connections, code);
		if (code)
		{
			error = std::format("cannot listen on {}:{}: {}", options.bind_address, options.port, code.message());
			asio::error_code ignored;
			m_acceptor.close(ignored);
			return false;
		}
		m_local_port = m_acceptor.local_endpoint(code).port();
		asio::post(m_acceptor.get_executor(), [self = shared_from_this()] { self->accept_next(); });
		return true;
	}

	void server::accept_next()
	{
		if (m_stopped || !m_acceptor.is_open())
			return;
		// Each accepted socket gets its own strand, so connections never wait on each other.
		m_acceptor.async_accept(asio::make_strand(m_context),
			[self = shared_from_this()](const asio::error_code& error, asio::ip::tcp::socket socket) {
				if (self->m_stopped)
					return;
				if (!error)
				{
					std::size_t open = 0;
					{
						std::scoped_lock lock(self->m_mutex);
						open = self->m_connections.size();
					}
					if (open >= self->m_options.max_connections)
					{
						asio::error_code ignored;
						const auto remote = socket.remote_endpoint(ignored);
						hb::log::warning("port {}: refused {} (at the {}-connection limit)", self->m_local_port,
							remote.address().to_string(), self->m_options.max_connections);
						socket.close(ignored);
						self->accept_next();
						return;
					}
					std::shared_ptr<connection> accepted = connection::create(std::move(socket), self->m_options.connection);
					std::weak_ptr<server> owner = self;
					std::weak_ptr<connection> weak = accepted;
					accepted->m_release = [owner, weak] {
						if (const auto alive = owner.lock())
							if (const auto released = weak.lock())
								alive->release(released);
					};
					{
						std::scoped_lock lock(self->m_mutex);
						self->m_connections.insert(accepted);
					}
					asio::post(accepted->executor(), [self, accepted] {
						if (self->m_on_accept)
							self->m_on_accept(accepted);
					});
				}
				else if (error == asio::error::operation_aborted)
					return;
				else
				{
					// Out of descriptors and the like: wait before trying again instead of spinning.
					hb::log::warning("port {}: accept failed: {}", self->m_local_port, error.message());
					self->m_retry_timer.expires_after(accept_retry_delay);
					self->m_retry_timer.async_wait([self](const asio::error_code& wait_error) {
						if (!wait_error)
							self->accept_next();
					});
					return;
				}
				self->accept_next();
			});
	}

	void server::release(const std::shared_ptr<connection>& released)
	{
		std::scoped_lock lock(m_mutex);
		m_connections.erase(released);
	}

	void server::stop()
	{
		asio::post(m_acceptor.get_executor(), [self = shared_from_this()] {
			self->m_stopped = true;
			self->m_retry_timer.cancel();
			asio::error_code ignored;
			self->m_acceptor.close(ignored);
		});
		std::vector<std::shared_ptr<connection>> open;
		{
			std::scoped_lock lock(m_mutex);
			open.assign(m_connections.begin(), m_connections.end());
		}
		for (const auto& owned : open)
			owned->close(close_reason::shutdown);
	}

	std::size_t server::connection_count() const
	{
		std::scoped_lock lock(m_mutex);
		return m_connections.size();
	}
}
