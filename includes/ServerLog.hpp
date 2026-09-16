#ifndef SERVER_LOG_HPP
#define SERVER_LOG_HPP

#include <cstddef>

// Best-effort diagnostics: fixed storage, no allocation and no blocking pipe I/O.
// The server owns the only poll(); flush() is called only after POLLOUT.
class ServerLog
{
	public:
		ServerLog();
		void initialize();
		int getFd() const;
		bool hasPending() const;
		void append(const char *level, const char *event, int fd,
			const char *nickname, const char *detail);
		void handlePoll(short events);

	private:
		ServerLog(const ServerLog &);
		ServerLog &operator=(const ServerLog &);
		void flush();
		bool _enabled;
		bool _dropped;
		size_t _size;
		char _buffer[16384];
};

#endif
