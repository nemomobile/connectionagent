
#include <QtCore/QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QDBusConnection>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "qconnectionmanager.h"
#include "connadaptor.h"

static void signal_handler(int signum)
{
    switch(signum) {
    case SIGHUP: exit(EXIT_FAILURE); break;
    case SIGTERM: exit(EXIT_SUCCESS); exit(0); break;
    }
}

static void daemonize(void)
{
    pid_t pid, sid;
    int fd;

    if ( getppid() == 1 ) return;

    signal(SIGHUP,signal_handler);
    signal(SIGTERM,signal_handler);

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }

    fd = open("/dev/null",O_RDWR, 0);

    if (fd != -1) {
        dup2 (fd, STDIN_FILENO);
        dup2 (fd, STDOUT_FILENO);
        dup2 (fd, STDERR_FILENO);

        if (fd > 2) {
            close (fd);
        }
    }

    umask(027);
}

int main(int argc, char *argv[])
{
    if (argc > 1)
       if (strcmp(argv[1],"-d") == 0) {
            daemonize();
       }

    QCoreApplication::setOrganizationName("Jolla Ltd");
    QCoreApplication::setOrganizationDomain("com.jollamobile");
    QCoreApplication::setApplicationName("connectionagent");
    QCoreApplication::setApplicationVersion("1.0");

    QCoreApplication a(argc, argv);

    QConnectionManager::instance();

    return a.exec();
}

