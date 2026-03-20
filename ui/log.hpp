#pragma once

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// Minimal log: 5 levels, color, tag from __FILE__. Thread-safe (fprintf to
// stderr is POSIX atomic per-call). No global objects, no init.
//
// Usage: -l <level>                  set global level (err/wrn/inf/dbg/dmp)
//        -l <comp>=<level>[,...]     set per-component level
//        e.g. -l dbg  -l solver_g=dbg,app=dmp

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

	struct Override {
		char tag[16];      // component name prefix (matched against stripped tag)
		Level level;
	};
	inline constexpr int MAX_OVERRIDES = 16;
	inline Override g_overrides[MAX_OVERRIDES]{};
	inline int g_n_overrides = 0;

	// parse -l argument: "dbg" or "solver_g=dbg,app=inf"
	inline bool parse(const char *spec)
	{
		auto parse_level = [](const char *s, int len) -> int {
			if(len == 3 && strncmp(s, "err", 3) == 0) return Err;
			if(len == 3 && strncmp(s, "wrn", 3) == 0) return Wrn;
			if(len == 3 && strncmp(s, "inf", 3) == 0) return Inf;
			if(len == 3 && strncmp(s, "dbg", 3) == 0) return Dbg;
			if(len == 3 && strncmp(s, "dmp", 3) == 0) return Dmp;
			return -1;
		};

		// try bare level first
		int len = (int)strlen(spec);
		int lvl = parse_level(spec, len);
		if(lvl >= 0) { g_level = (Level)lvl; return true; }

		// parse comma-separated comp=level pairs
		const char *p = spec;
		while(*p) {
			const char *eq = strchr(p, '=');
			if(!eq) return false;
			const char *comma = strchr(eq + 1, ',');
			int vlen = comma ? (int)(comma - eq - 1) : (int)strlen(eq + 1);
			lvl = parse_level(eq + 1, vlen);
			if(lvl < 0) return false;

			if(g_n_overrides < MAX_OVERRIDES) {
				auto &o = g_overrides[g_n_overrides++];
				int tlen = (int)(eq - p);
				if(tlen > 15) tlen = 15;
				memcpy(o.tag, p, tlen);
				o.tag[tlen] = 0;
				o.level = (Level)lvl;
			}
			p = comma ? comma + 1 : eq + 1 + vlen;
		}
		return true;
	}
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
	// strip path prefix from tag
	const char *p = tag;
	for(const char *s = tag; *s; s++)
		if(*s == '/') p = s + 1;

	// determine effective level: check overrides first
	Log::Level threshold = Log::g_level;
	for(int i = 0; i < Log::g_n_overrides; i++) {
		if(strncmp(p, Log::g_overrides[i].tag, strlen(Log::g_overrides[i].tag)) == 0) {
			threshold = Log::g_overrides[i].level;
			break;
		}
	}

	if(level > threshold) return;
	auto &li = g_log_info[level];

	char buf[256];
	va_list va;
	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	fprintf(stderr, "%s%s %8.8s> %s\033[0m\n", li.color, li.label, p, buf);
}
