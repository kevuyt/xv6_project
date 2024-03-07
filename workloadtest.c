#include "types.h"
#include "user.h"
#include "fcntl.h"

int scheduledUptime[4], scheduledTicks[4];


void runCommand(char *command, char **argv, int index) {
    int pid = fork();
    if (pid < 0) {
        printf(1, "Fork failed\n");
        return;
    }
    if (pid == 0) {
        pid = fork();
        if (pid == 0) {
            exec(command, argv);
            printf(1, "exec %s failed\n", command);
        }
        else if (index < 4) {
            while (wait() > 0) sleep(1);
            printf(1, "\n\n\n%s Turnaround Uptime: %d\n\n\n", command, uptime() - scheduledUptime[index]);
            exit();
        }
        else {
            while (wait() > 0) sleep(1);
            exit();
        }
    }

}

int main(void) {

    char *stressfs_argv[] = {"stressfs", 0};
    scheduledUptime[0] = uptime();
    scheduledTicks[0] = ticks_running(getpid());
    runCommand("stressfs", stressfs_argv, 0);

    char *find_argv[] = {"find", "/", 0};
    scheduledUptime[1] = uptime();
    scheduledTicks[1] = ticks_running(getpid());
    runCommand("find", find_argv, 1);

    char *cat_argv[] = {"cat", "README", 0};
    scheduledUptime[2] = uptime();
    scheduledTicks[2] = ticks_running(getpid());
    runCommand("cat", cat_argv, 2); // Assuming the 'uniq' part is handled in shell

    char *sleep_argv[] = {"sleep", "100", 0};
    scheduledUptime[3] = uptime();
    scheduledTicks[3] = ticks_running(getpid());
    runCommand("sleep", sleep_argv, 4);
    runCommand("sleep", sleep_argv, 4);
    runCommand("sleep", sleep_argv, 3);


    while (wait() > 0) sleep(1);
    exit();
}