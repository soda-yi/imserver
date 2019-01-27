#include "imserver.hpp"

#include <sys/prctl.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;
char **g_argv;

/*
 * move environ to other place
 */
int initproctitle()
{
    int env_size;
    for (env_size = 0; environ[env_size]; ++env_size) {
    }

    char **env_n = (char **)malloc(sizeof(char *) * (env_size + 1));

    int env_r_size = 0;
    for (int i = 0; environ[i]; ++i) {
        int length = strlen(environ[i]) + 1;
        env_n[i] = (char *)malloc(sizeof(char) * length);
        strcpy(env_n[i], environ[i]);
        env_r_size += length;
    }
    env_n[env_size] = NULL;
    environ = env_n;

    return env_r_size;
}

/*
 * set argv[0] and move argv[i](i>0)
 */
void setproctitle(const char *title)
{
    int move_dif = strlen(title) - strlen(g_argv[0]),
        move_size = 0;
    if (move_dif == 0) {
        strcpy(g_argv[0], title);
        return;
    }

    for (int i = 1; g_argv[i]; ++i) {
        move_size += strlen(g_argv[i]) + 1;
    }

    int l0 = strlen(g_argv[0]);
    char *argv1 = g_argv[0] + l0 + 1;
    memmove(argv1 + move_dif, argv1, move_size * sizeof(char));
    if (move_dif < 0) {
        memset(argv1 + move_size + move_dif, 0, -move_dif * sizeof(char));
    }
    for (int i = 1; g_argv[i]; ++i) {
        g_argv[i] += move_dif;
    }

    strcpy(g_argv[0], title);
}

int main(int argc, char **argv)
{
    g_argv = argv;
    if (daemon(1, 1) < 0) {
        //if(0){
        perror("error daemon.../n");
        exit(1);
    }

    initproctitle();
    setproctitle("IMServerd_safe");

    signal(SIGCHLD, SIG_IGN);

    do {
        pid_t pid = fork();
        if (pid == 0) {
            setproctitle("IMServerd");
            prctl(PR_SET_PDEATHSIG, SIGHUP);

            herry::imserver::IMServer server("0.0.0.0", 22888);
            server.Main();

            delete[] environ;
            return 0;
        } else {
            waitpid(pid, NULL, 0);
        }
    } while (1);

    delete[] environ;
    return 0;
}
