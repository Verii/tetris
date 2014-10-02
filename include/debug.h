#ifndef DEBUG_H_
#define DEBUG_H_

#include <stdio.h>

/* Prints messages to stderr of the form:
 * [TIME/DATE] message
 */
void debug_log(const char *fmt, ...);

#ifdef DEBUG
#define debug(M, ...)	 debug_log("[DEBUG] " M, ##__VA_ARGS__)
#else
#define debug(M, ...)
#endif				/* DEBUG */

#define log_err(M, ...)  do { \
	debug_log("[ERR] " M " (%s:%d)", \
		##__VA_ARGS__, __FILE__, __LINE__); \
	fflush(NULL); \
	} while(0)

#define log_warn(M, ...) debug_log("[WARN] " M " (%s:%d)", ##__VA_ARGS__, \
		__FILE__, __LINE__)

#define log_info(M, ...) debug_log("[INFO] " M " (%s:%d)", ##__VA_ARGS__, \
		__FILE__, __LINE__)

#endif				/* DEBUG_H_ */
