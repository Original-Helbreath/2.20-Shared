#pragma once

#include "asio_config.h"

#include <chrono>
#include <functional>
#include <memory>

// Calls a function every interval on the given executor (a connection's strand for heartbeats) until
// stopped. Idle timeouts live on hb::net::connection itself (set_idle_timeout).
namespace hb::net
{
	class periodic_timer : public std::enable_shared_from_this<periodic_timer>
	{
	public:
		static std::shared_ptr<periodic_timer> create(asio::any_io_executor executor)
		{
			return std::shared_ptr<periodic_timer>(new periodic_timer(std::move(executor)));
		}

		// First call after one interval.
		void start(std::chrono::milliseconds interval, std::function<void()> tick)
		{
			asio::post(m_timer.get_executor(), [self = shared_from_this(), interval, tick = std::move(tick)]() mutable {
				self->m_interval = interval;
				self->m_tick = std::move(tick);
				self->m_running = true;
				self->arm();
			});
		}

		void stop()
		{
			asio::post(m_timer.get_executor(), [self = shared_from_this()] {
				self->m_running = false;
				self->m_tick = nullptr;
				self->m_timer.cancel();
			});
		}

	private:
		explicit periodic_timer(asio::any_io_executor executor)
			: m_timer(std::move(executor))
		{
		}

		void arm()
		{
			m_timer.expires_after(m_interval);
			m_timer.async_wait([self = shared_from_this()](const asio::error_code& error) {
				if (error || !self->m_running)
					return;
				if (self->m_tick)
					self->m_tick();
				if (self->m_running)
					self->arm();
			});
		}

		asio::steady_timer m_timer;
		std::chrono::milliseconds m_interval{ 1000 };
		std::function<void()> m_tick;
		bool m_running = false;
	};
}
