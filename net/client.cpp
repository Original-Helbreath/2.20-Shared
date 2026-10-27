#include "client.h"

#include <asio/bind_executor.hpp>

#include <format>
#include <utility>

namespace hb::net
{
	std::shared_ptr<client> client::create(asio::io_context& context, const client_options& options)
	{
		return std::shared_ptr<client>(new client(context, options));
	}

	client::client(asio::io_context& context, const client_options& options)
		: m_context(context)
		, m_strand(asio::make_strand(context))
		, m_resolver(m_strand)
		, m_retry_timer(m_strand)
		, m_options(options)
	{
	}

	void client::start(connect_handler on_connect, failure_handler on_failure)
	{
		asio::post(m_strand, [self = shared_from_this(), on_connect = std::move(on_connect), on_failure = std::move(on_failure)]() mutable {
			self->m_on_connect = std::move(on_connect);
			self->m_on_failure = std::move(on_failure);
			self->attempt();
		});
	}

	void client::stop()
	{
		asio::post(m_strand, [self = shared_from_this()] {
			self->m_stopped = true;
			self->m_resolver.cancel();
			self->m_retry_timer.cancel();
			std::shared_ptr<connection> open;
			{
				std::scoped_lock lock(self->m_mutex);
				open = self->m_current;
			}
			if (open)
				open->close(close_reason::shutdown);
		});
	}

	std::shared_ptr<connection> client::current() const
	{
		std::scoped_lock lock(m_mutex);
		return m_current;
	}

	void client::attempt()
	{
		if (m_stopped)
			return;
		m_resolver.async_resolve(m_options.host, std::to_string(m_options.port),
			asio::bind_executor(m_strand, [self = shared_from_this()](const asio::error_code& error,
											  asio::ip::tcp::resolver::results_type endpoints) {
				if (self->m_stopped)
					return;
				if (error)
				{
					if (self->m_on_failure)
						self->m_on_failure(std::format("cannot resolve {}: {}", self->m_options.host, error.message()));
					self->schedule_retry();
					return;
				}
				auto socket = std::make_shared<asio::ip::tcp::socket>(asio::make_strand(self->m_context));
				asio::async_connect(*socket, endpoints,
					asio::bind_executor(self->m_strand, [self, socket](const asio::error_code& connect_error, const asio::ip::tcp::endpoint&) {
						if (self->m_stopped)
							return;
						if (connect_error)
						{
							if (self->m_on_failure)
								self->m_on_failure(std::format("cannot connect to {}:{}: {}", self->m_options.host, self->m_options.port,
									connect_error.message()));
							self->schedule_retry();
							return;
						}
						std::shared_ptr<connection> opened = connection::create(std::move(*socket), self->m_options.connection);
						std::weak_ptr<client> owner = self;
						opened->m_release = [owner] {
							if (const auto alive = owner.lock())
								asio::post(alive->m_strand, [alive] { alive->released(); });
						};
						{
							std::scoped_lock lock(self->m_mutex);
							self->m_current = opened;
						}
						asio::post(opened->executor(), [self, opened] {
							if (self->m_on_connect)
								self->m_on_connect(opened);
						});
					}));
			}));
	}

	void client::released()
	{
		{
			std::scoped_lock lock(m_mutex);
			m_current.reset();
		}
		schedule_retry();
	}

	void client::schedule_retry()
	{
		if (m_stopped || !m_options.reconnects)
			return;
		m_retry_timer.expires_after(m_options.retry_delay);
		m_retry_timer.async_wait([self = shared_from_this()](const asio::error_code& error) {
			if (!error)
				self->attempt();
		});
	}
}
