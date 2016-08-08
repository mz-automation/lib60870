/*
 * lib60870_config.h
 */

#ifndef CONFIG_LIB60870_CONFIG_H_
#define CONFIG_LIB60870_CONFIG_H_

/* include asserts if set to 1 */
#define DEBUG 0

/* print debugging information with printf if set to 1 */
#define DEBUG_SOCKET 0

/* activate TCP keep alive mechanism. 1 -> activate */
#define CONFIG_ACTIVATE_TCP_KEEPALIVE 1

/* time (in s) between last message and first keepalive message */
#define CONFIG_TCP_KEEPALIVE_IDLE 5

/* time between subsequent keepalive messages if no ack received */
#define CONFIG_TCP_KEEPALIVE_INTERVAL 2

/* number of not missing keepalive responses until socket is considered dead */
#define CONFIG_TCP_KEEPALIVE_CNT 2

#endif /* CONFIG_LIB60870_CONFIG_H_ */
