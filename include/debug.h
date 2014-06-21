#ifndef DEBUG_H_
#define DEBUG_H_

void _debug_log (const char *fmt, ...);

#ifdef DEBUG
#define debug(M, ...)	 _debug_log("DEBUG :: " M, ##__VA_ARGS__)
#else
#define debug(M, ...)
#endif

#define log_err(M, ...)  _debug_log("[ERROR] " M " :(%s:%d %s)", \
				__FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define log_warn(M, ...) _debug_log("[WARN] " M " :(%s:%d %s)", \
				__FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define log_info(M, ...) _debug_log("[INFO] " M " :(%s:%d %s)", \
				__FILE__, __LINE__, __func__, ##__VA_ARGS__)

#endif	/* DEBUG_H_ */
