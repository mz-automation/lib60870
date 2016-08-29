#include <stdlib.h>
#include <stdbool.h>

#include "iec60870_slave.h"

#include "hal_thread.h"

static bool running = true;

int
main(int argc, char** argv)
{



    Master master = T104Master_create(NULL);

    Master_start(master);

    while (running) {
        Thread_sleep(10000);
    }

    Master_stop(master);

}
