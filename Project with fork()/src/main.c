#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>


void exit_handler();
void switch_handler();

char main_loop_flag = 1;
char refresh_speed = 1;

int main(int argc, const char **argv) {
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    int pid = 0;
    int ppid = getpid();


    if ((pid = fork()) < 0) {
        perror("fork");
        exit(-1);
    }

    if (pid != 0) { /* parent */
        signal(SIGUSR1, exit_handler);
        signal(SIGUSR2, switch_handler);
        char output[32];
        while (main_loop_flag) {
            sprintf(output, "Alarm after %d seconds\n", refresh_speed);
            write(STDOUT_FILENO, output, strlen(output));
            sleep(refresh_speed);
        }

    } else { /* child */
        char input = 0;
        char listen_flag = 1;

        while (listen_flag) {
            char is_not_eof = 1;
            is_not_eof = read(STDIN_FILENO, &input, 1);

            if (!is_not_eof) {
                kill(ppid, SIGUSR1);
                listen_flag = 0;
            }

            if (input == '\n') {
                kill(ppid, SIGUSR2);
            }
        }
    }
    return 0;
}

void exit_handler() {
    main_loop_flag = 0;
}

void switch_handler() {
    signal(SIGUSR2, switch_handler); /* reset signal */
    refresh_speed = refresh_speed == 1 ? 3 : 1;
}
