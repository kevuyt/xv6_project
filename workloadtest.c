#include "types.h"
#include "user.h"

void run_and_measure(const char *command) {
    int start_uptime, end_uptime;

    printf(1, "\nExecuting: %s\n", command);

    start_uptime = uptime();

    if (fork() == 0) {
        char *argv[] = {"sh", "-c", (char *) command, 0};
        exec("sh", argv);
        printf(2, "exec %s failed\n", command);
        exit();
    } else {
        wait();
    }

    end_uptime = uptime();

    printf(1, "Completed: %s\n", command);
    printf(1, "Ticks Running: %d\n", ticks_running(getpid()));
    printf(1, "Uptime: %d seconds\n", end_uptime - start_uptime);
}

int main(void) {
    printf(1, "Starting scheduler tests...\n");

    run_and_measure("ls");
    run_and_measure("find /");
    run_and_measure("cat README | uniq");
    run_and_measure("YOUR_NON_TRIVIAL_COMMAND_HERE"); // Replace with your command

    printf(1, "Scheduler tests completed.\n");

    exit();
}
