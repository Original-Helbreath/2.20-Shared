#pragma once

#include <format>
#include <string>
#include <string_view>
#include <utility>

// hb::log per Standards sect. 13, shared by the Login server, the World server and the client.
namespace hb::log
{
	enum class level
	{
		debug,
		info,
		warning,
		error
	};

	// First line of main(): opens <application>.log (truncated) and installs the terminate handler.
	void initialize(std::string_view application, std::string_view component);
	void shutdown();

	// First line of every other thread's body: MSVC keeps the terminate handler per thread, so a
	// worker without it would crash without runtime_error.log.
	void install_thread_crash_handler();

	void set_level(level minimum);
	bool parse_level(std::string_view text, level& result);

	// Writes runtime_error.log and logs the crash as an error.
	void report_crash(std::string_view reason);

	void write(level message_level, std::string_view message);

	// For a level chosen at run time: hb::log::write(level, "closed: {}", reason).
	template <typename... T>
		requires(sizeof...(T) > 0)
	void write(level message_level, std::format_string<T...> format, T&&... args)
	{
		write(message_level, std::string_view(std::format(format, std::forward<T>(args)...)));
	}

	template <typename... T>
	void debug(std::format_string<T...> format, T&&... args)
	{
		write(level::debug, std::format(format, std::forward<T>(args)...));
	}

	template <typename... T>
	void info(std::format_string<T...> format, T&&... args)
	{
		write(level::info, std::format(format, std::forward<T>(args)...));
	}

	template <typename... T>
	void warning(std::format_string<T...> format, T&&... args)
	{
		write(level::warning, std::format(format, std::forward<T>(args)...));
	}

	template <typename... T>
	void error(std::format_string<T...> format, T&&... args)
	{
		write(level::error, std::format(format, std::forward<T>(args)...));
	}
}
