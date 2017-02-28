/*
 * lib60870_config.h
 */

#ifndef CONFIG_LIB60870_CONFIG_H_
#define CONFIG_LIB60870_CONFIG_H_


/* print debugging information with printf if set to 1 */
#define CONFIG_DEBUG_OUTPUT 1

/* activate TCP keep alive mechanism. 1 -> activate */
#define CONFIG_ACTIVATE_TCP_KEEPALIVE 1

/* time (in s) between last message and first keepalive message */
#define CONFIG_TCP_KEEPALIVE_IDLE 5

/* time between subsequent keepalive messages if no ack received */
#define CONFIG_TCP_KEEPALIVE_INTERVAL 2

/* number of not missing keepalive responses until socket is considered dead */
#define CONFIG_TCP_KEEPALIVE_CNT 2

/**
 * Use static memory for the slave (outstation) message queue.
 *
 * Note: Use only when statically linking the library. You can only have
 * a single slave instance!
 * */
#define CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE 0

/**
 * Compile the slave/server stack using threads. This will require semaphores also
 */
#define CONFIG_SLAVE_USING_THREADS 1

/**
 * Define the default size for the slave (outstation) message queue.
 *
 * For each queued message about 256 bytes of memory are required.
 */
#define CONFIG_SLAVE_MESSAGE_QUEUE_SIZE 10

#endif /* CONFIG_LIB60870_CONFIG_H_ */
