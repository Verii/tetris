#ifndef DEBUG_H_
#define DEBUG_H_

void debug_log (const char *fmt, ...);

#ifdef DEBUG
#define debug(M, ...)	 debug_log("DEBUG :: " M, __VA_ARGS__)
#else
#define debug(M, ...)
#endif /* DEBUG */

#define log_err(M, ...)  do { \
	debug_log("[ERROR] " M " (%s:%d %s)", \
		__VA_ARGS__, __FILE__, __LINE__, __func__); \
	fflush (NULL); \
	} while(0)

#define log_warn(M, ...) do { \
	debug_log("[WARN] " M " (%s:%d %s)", \
		__VA_ARGS__, __FILE__, __LINE__, __func__); \
	fflush (NULL); \
	} while(0)

#define log_info(M, ...) do { \
	debug_log("[INFO] " M " (%s:%d %s)", \
		__VA_ARGS__, __FILE__, __LINE__, __func__); \
	fflush (NULL); \
	} while(0)

#endif	/* DEBUG_H_ */
