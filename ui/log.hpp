#pragma once

#include <stdio.h>
#include <stdarg.h>

// Minimal log: 5 levels, color, tag from __FILE__. Thread-safe (fprintf to
// stderr is POSIX atomic per-call). No global objects, no init.

#ifndef LOG_TAG
#define LOG_TAG __FILE__
#endif

#define lerr(...) log_write(Log::Err, LOG_TAG, __VA_ARGS__)
#define lwrn(...) log_write(Log::Wrn, LOG_TAG, __VA_ARGS__)
#define linf(...) log_write(Log::Inf, LOG_TAG, __VA_ARGS__)
#define ldbg(...) log_write(Log::Dbg, LOG_TAG, __VA_ARGS__)
#define ldmp(...) log_write(Log::Dmp, LOG_TAG, __VA_ARGS__)

namespace Log {
	enum Level { Err, Wrn, Inf, Dbg, Dmp };
	inline Level g_level = Inf;
}

struct log_info { const char *label; const char *color; };

inline constexpr log_info g_log_info[] = {
	{ "err", "\033[31m"   },
	{ "wrn", "\033[33m"   },
	{ "inf", "\033[1m"    },
	{ "dbg", "\033[22m"   },
	{ "dmp", "\033[1;30m" },
};

__attribute__((format(printf, 3, 4)))
inline void log_write(Log::Level level, const char *tag, const char *fmt, ...)
{
	if(level > Log::g_level) return;
	auto &li = g_log_info[level];

	// strip path prefix from tag
	const char *p = tag;
	for(const char *s = tag; *s; s++)
		if(*s == '/') p = s + 1;

	char buf[256];
	va_list va;
	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	fprintf(stderr, "%s%s %8.8s> %s\033[0m\n", li.color, li.label, p, buf);
}
