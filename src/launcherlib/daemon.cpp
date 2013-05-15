/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (directui@nokia.com)
**
** This file is part of applauncherd
**
** If you have questions regarding the use of this file, please contact
** Nokia at directui@nokia.com.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#include "daemon.h"
#include "logger.h"
#include "connection.h"
#include "booster.h"
#include "boosterfactory.h"
#include "boosterpluginregistry.h"
#include "singleinstance.h"
#include "socketmanager.h"

#include <cstdlib>
#include <cerrno>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <glob.h>
#include <cstring>
#include <cstdio>

Daemon * Daemon::m_instance = NULL;
int Daemon::m_lockFd = -1;
const int Daemon::m_boosterSleepTime = 2;

Daemon::Daemon(int & argc, char * argv[]) :
    m_daemon(false),
    m_quiet(false),
    m_bootMode(false),
    m_socketManager(new SocketManager),
    m_singleInstance(new SingleInstance)
{
    if (!Daemon::m_instance)
    {
        Daemon::m_instance = this;
    }
    else
    {
        Logger::logErrorAndDie(EXIT_FAILURE, "Daemon: Daemon already created!\n");
    }

    // Parse arguments
    parseArgs(ArgVect(argv, argv + argc));

    // Disable console output
    if (m_quiet)
        consoleQuiet();

    // Store arguments list
    m_initialArgv = argv;
    m_initialArgc = argc;

    if (pipe(m_boosterPipeFd) == -1)
    {
        Logger::logErrorAndDie(EXIT_FAILURE, "Daemon: Creating a pipe for boosters failed!\n");
    }

    if (pipe(m_sigChldPipeFd) == -1)
    {
        Logger::logErrorAndDie(EXIT_FAILURE, "Daemon: Creating a pipe for SIGCHLD failed!\n");
    }

    if (pipe(m_sigTermPipeFd) == -1)
    {
        Logger::logErrorAndDie(EXIT_FAILURE, "Daemon: Creating a pipe for SIGTERM failed!\n");
    }

    if (pipe(m_sigUsr1PipeFd) == -1)
    {
        Logger::logErrorAndDie(EXIT_FAILURE, "Daemon: Creating a pipe for SIGUSR1 failed!\n");
    }

    // Daemonize if desired
    if (m_daemon)
    {
        daemonize();
    }
}

void Daemon::consoleQuiet()
{
    close(0);
    close(1);
    close(2);

    if (open("/dev/null", O_RDONLY) < 0)
        Logger::logErrorAndDie(EXIT_FAILURE, "Daemon: Failed to open /dev/null as read-only");

    int fd = open("/dev/null", O_WRONLY);
    if ((fd == -1) || (dup(fd) < 0))
        Logger::logErrorAndDie(EXIT_FAILURE, "Daemon: Failed to open /dev/null as write-only");
}

Daemon * Daemon::instance()
{
    return Daemon::m_instance;
}

bool Daemon::lock()
{
    struct flock fl;

    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;

    if((m_lockFd = open("/tmp/applauncherd.lock", O_WRONLY | O_CREAT, 0666)) == -1)
        return false;

    if(fcntl(m_lockFd, F_SETLK, &fl) == -1)
        return false;

    return true;
}

void Daemon::unlock()
{
    if (m_lockFd != -1)
    {
        close(m_lockFd);
        m_lockFd = -1;
    }
}

void Daemon::initBoosterSockets()
{
    const int numBoosters = BoosterPluginRegistry::pluginCount();
    for (int i = 0; i < numBoosters; i++)
    {
        BoosterPluginEntry * plugin = BoosterPluginRegistry::pluginEntry(i);
        if (plugin)
        {
            Logger::logDebug("Daemon: initing socket: %s", plugin->socketNameFunc());
            m_socketManager->initSocket(plugin->socketNameFunc());
        }
    }
}

void Daemon::forkBoosters()
{
    const int numBoosters = BoosterPluginRegistry::pluginCount();
    for (int i = 0; i < numBoosters; i++)
    {
        BoosterPluginEntry * plugin = BoosterPluginRegistry::pluginEntry(i);
        if (plugin)
        {
            Logger::logDebug("Daemon: forking booster: '%c'", plugin->type);
            forkBooster(plugin->type);
        }
    }
}

void Daemon::run()
{
    // Make sure that LD_BIND_NOW does not prevent dynamic linker to
    // use lazy binding in later dlopen() calls.
    unsetenv("LD_BIND_NOW");

    // dlopen() booster plugins
    loadBoosterPlugins();

    // Create sockets for each of the boosters
    initBoosterSockets();

    // dlopen single-instance
    loadSingleInstancePlugin();

    // Fork each booster for the first time
    forkBoosters();

    // Main loop
    while (true)
    {
        // Variables used by the select call
        fd_set rfds;
        int ndfs = 0;

        // Init data for select
        FD_ZERO(&rfds);

        FD_SET(m_boosterPipeFd[0], &rfds);
        ndfs = std::max(ndfs, m_boosterPipeFd[0]);

        FD_SET(m_sigChldPipeFd[0], &rfds);
        ndfs = std::max(ndfs, m_sigChldPipeFd[0]);

        FD_SET(m_sigTermPipeFd[0], &rfds);
        ndfs = std::max(ndfs, m_sigTermPipeFd[0]);

        FD_SET(m_sigUsr1PipeFd[0], &rfds);
        ndfs = std::max(ndfs, m_sigUsr1PipeFd[0]);

        char buffer;

        // Wait for something appearing in the pipes.
        if (select(ndfs + 1, &rfds, NULL, NULL, NULL) > 0)
        {
            Logger::logDebug("select done.");

            // Check if a booster died
            if (FD_ISSET(m_boosterPipeFd[0], &rfds))
            {
                Logger::logDebug("FD_ISSET(m_boosterPipeFd[0])");
                readFromBoosterPipe(m_boosterPipeFd[0]);
            }

            // Check if we got SIGCHLD
            if (FD_ISSET(m_sigChldPipeFd[0], &rfds))
            {
                Logger::logDebug("FD_ISSET(m_sigChldPipeFd[0])");
                read(m_sigChldPipeFd[0], &buffer, 1);
                reapZombies();
            }

            // Check if we got SIGTERM
            if (FD_ISSET(m_sigTermPipeFd[0], &rfds))
            {
                Logger::logDebug("FD_ISSET(m_sigTermPipeFd[0])");
                read(m_sigTermPipeFd[0], &buffer, 1);
                exit(EXIT_SUCCESS);
            }

            // Check if we got SIGUSR1
            if (FD_ISSET(m_sigUsr1PipeFd[0], &rfds))
            {
                Logger::logDebug("FD_ISSET(m_sigUsr1PipeFd[0])");
                read(m_sigUsr1PipeFd[0], &buffer, 1);
                enterNormalMode();
            }
        }
    }
}

void Daemon::readFromBoosterPipe(int fd)
{
    // There should first appear a character representing the type of a
    // booster and then pid of the invoker process that communicated with
    // that booster.
    char msg;
    ssize_t count = read(fd, static_cast<void *>(&msg), 1);
    if (count)
    {
        // Read pid of peer invoker
        pid_t invokerPid;
        count = read(fd, static_cast<void *>(&invokerPid), sizeof(pid_t));

        if (count < static_cast<ssize_t>(sizeof(pid_t)))
        {
            Logger::logErrorAndDie(EXIT_FAILURE, "Daemon: pipe connection with booster failed");
        }
        else
        {
            Logger::logDebug("Daemon: invoker's pid: %d \n", invokerPid);
        }

        if (invokerPid != 0)
        {
            // Store booster - invoker pid pair
            pid_t boosterPid = boosterPidForType(msg);
            if (boosterPid)
            {
                m_boosterPidToInvokerPid[boosterPid] = invokerPid;
            }
        }

        // Read booster respawn delay
        int delay;
        count = read(fd, static_cast<void *>(&delay), sizeof(int));

        if (count < static_cast<ssize_t>(sizeof(int)))
        {
            Logger::logErrorAndDie(EXIT_FAILURE, "Daemon: pipe connection with booster failed");
        }
        else
        {
            Logger::logDebug("Daemon: respawn delay received: %d \n", delay);
        }

        // Fork a new booster of the given type

        // 2nd param guarantees some time for the just launched application
        // to start up before forking new booster. Not doing this would
        // slow down the start-up significantly on single core CPUs.

        forkBooster(msg, delay);
    }
    else
    {
        Logger::logWarning("Daemon: Nothing read from the pipe\n");
    }
}

void Daemon::killProcess(pid_t pid, int signal) const
{
    if (pid)
    {
        Logger::logDebug("Daemon: Killing pid %d with %d", pid, signal);
        if (kill(pid, signal) != 0)
        {
            Logger::logError("Daemon: Failed to kill %d: %s\n",
                             pid, strerror(errno));
        }
    }
}

void Daemon::loadSingleInstancePlugin()
{
    void * handle = dlopen(SINGLE_INSTANCE_PATH, RTLD_NOW);
    if (!handle)
    {
        Logger::logWarning("Daemon: dlopening single-instance failed: %s", dlerror());
    }
    else
    {
        if (m_singleInstance->validateAndRegisterPlugin(handle))
        {
            Logger::logDebug("Daemon: single-instance plugin loaded.'");
        }
        else
        {
            Logger::logWarning("Daemon: Invalid single-instance plugin: '%s'",
                               SINGLE_INSTANCE_PATH);
        }
    }
}

void Daemon::loadBoosterPlugins()
{
    const char*        PATTERN = "lib*booster.so";
    const unsigned int BUF_LEN = 256;

    if (strlen(PATTERN) + strlen(BOOSTER_PLUGIN_DIR) + 2 > BUF_LEN)
    {
        Logger::logError("Daemon: path to plugins too long");
        return;
    }

    char buffer[BUF_LEN];
    memset(buffer, 0, BUF_LEN);
    strcpy(buffer, BOOSTER_PLUGIN_DIR);
    strcat(buffer, "/");
    strcat(buffer, PATTERN);

    // get full path to all plugins
    glob_t globbuf;
    if (glob(buffer, 0, NULL, &globbuf) == 0)
    {
        for (__size_t i = 0; i < globbuf.gl_pathc; i++)
        {
            void *handle = dlopen(globbuf.gl_pathv[i], RTLD_NOW | RTLD_GLOBAL);
            if (!handle)
            {
                Logger::logWarning("Daemon: dlopening booster failed: %s", dlerror());
            }
            else
            {
                char newType = BoosterPluginRegistry::validateAndRegisterPlugin(handle);
                if (newType)
                {
                    Logger::logDebug("Daemon: Booster of type '%c' loaded.'", newType);
                }
                else
                {
                    Logger::logWarning("Daemon: Invalid booster plugin: '%s'", buffer);
                }
            }
        }
        globfree(&globbuf);
    }
    else
    {
        Logger::logError("Daemon: can't find booster plugins");
    }
}

void Daemon::forkBooster(char type, int sleepTime)
{
    // Invalidate current booster pid for the given type
    setPidToBooster(type, 0);

    // Fork a new process
    pid_t newPid = fork();

    if (newPid == -1)
        Logger::logErrorAndDie(EXIT_FAILURE, "Daemon: Forking while invoking");

    if (newPid == 0) /* Child process */
    {
        // Reset used signal handlers
        signal(SIGCHLD, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGUSR1, SIG_DFL);

        // Will get this signal if applauncherd dies
        prctl(PR_SET_PDEATHSIG, SIGHUP);

        // Close unused read end of the booster pipe
        close(m_boosterPipeFd[0]);

        // Close SIGCHLD pipe
        close(m_sigChldPipeFd[0]);
        close(m_sigChldPipeFd[1]);

        // Close SIGTERM pipe
        close(m_sigTermPipeFd[0]);
        close(m_sigTermPipeFd[1]);

        // Close SIGUSR1 pipe
        close(m_sigUsr1PipeFd[0]);
        close(m_sigUsr1PipeFd[1]);

        // Close unused sockets inherited from daemon
        closeUnusedSockets(type);

        // Close lock file, it's not needed in the booster
        Daemon::unlock();

        // Set session id
        if (setsid() < 0)
            Logger::logError("Daemon: Couldn't set session id\n");

        // Guarantee some time for the just launched application to
        // start up before initializing new booster if needed.
        // Not done if in the boot mode.
        if (!m_bootMode && sleepTime)
            sleep(sleepTime);

        Logger::logDebug("Daemon: Running a new Booster of type '%c'", type);

        // Create a new booster, initialize and run it
        Booster * booster = BoosterFactory::create(type);
        if (booster)
        {
            // Initialize and wait for commands from invoker
            booster->initialize(m_initialArgc, m_initialArgv, m_boosterPipeFd,
                                m_socketManager->findSocket(booster->socketId().c_str()),
                                m_singleInstance, m_bootMode);

            // Run the current Booster
            booster->run(m_socketManager);

            // Finish
            delete booster;

            // _exit() instead of exit() to avoid situation when destructors
            // for static objects may be run incorrectly
            _exit(EXIT_SUCCESS);
        }
        else
        {
            Logger::logErrorAndDie(EXIT_FAILURE, "Daemon: Unknown booster type '%c'\n", type);
        }
    }
    else /* Parent process */
    {
        // Store the pid so that we can reap it later
        m_children.push_back(newPid);

        // Set current process ID globally to the given booster type
        // so that we now which booster to restart when booster exits.
        setPidToBooster(type, newPid);
    }
}

void Daemon::reapZombies()
{
    // Loop through all child pid's and wait for them with WNOHANG.
    PidVect::iterator i(m_children.begin());
    while (i != m_children.end())
    {
        // Check if the pid had exited and become a zombie
        int status;
        pid_t pid = waitpid(*i, &status, WNOHANG);
        if (pid)
        {
            // The pid had exited. Remove it from the pid vector.
            i = m_children.erase(i);

            // Find out if the exited process has a mapping with an invoker process.
            // If this is the case, then kill the invoker process with the same signal
            // that killed the exited process.
            PidMap::iterator it = m_boosterPidToInvokerPid.find(pid);
            if (it != m_boosterPidToInvokerPid.end())
            {
                Logger::logDebug("Daemon: Terminated process had a mapping to an invoker pid");

                if (WIFSIGNALED(status))
                {
                    int signal = WTERMSIG(status);
                    pid_t invokerPid = (*it).second;

                    Logger::logDebug("Daemon: Booster (pid=%d) was terminated due to signal %d\n", pid, signal);
                    Logger::logDebug("Daemon: Killing invoker process (pid=%d) by signal %d..\n", invokerPid, signal);

                    if (kill(invokerPid, signal) != 0)
                    {
                        Logger::logError("Daemon: Failed to send signal %d to invoker: %s\n", signal, strerror(errno));
                    }
                }

                // Remove a dead booster
                m_boosterPidToInvokerPid.erase(it);
            }

            // Check if pid belongs to a booster and restart the dead booster if needed
            char type = boosterTypeForPid(pid);
            if (type != 0)
            {
                forkBooster(type, m_boosterSleepTime);
            }
        }
        else
        {
            i++;
        }
    }
}

void Daemon::setPidToBooster(char type, pid_t pid)
{
    m_boosterTypeToPid[type] = pid;
}

char Daemon::boosterTypeForPid(pid_t pid) const
{
    TypeMap::const_iterator i = m_boosterTypeToPid.begin();
    while (i != m_boosterTypeToPid.end())
    {
        if (i->second == pid)
        {
            return i->first;
        }

        i++;
    }

    return 0;
}

pid_t Daemon::boosterPidForType(char type) const
{
    TypeMap::const_iterator i = m_boosterTypeToPid.find(type);
    return i == m_boosterTypeToPid.end() ? 0 : i->second;
}

void Daemon::closeUnusedSockets(char type)
{
    const int numBoosters = BoosterPluginRegistry::pluginCount();
    for (int i = 0; i < numBoosters; i++)
    {
        BoosterPluginEntry * plugin = BoosterPluginRegistry::pluginEntry(i);
        if (plugin && (plugin->type != type))
        {
            m_socketManager->closeSocket(plugin->socketNameFunc());
        }
    }
}

void Daemon::daemonize()
{
    // Our process ID and Session ID
    pid_t pid, sid;

    // Fork off the parent process: first fork
    pid = fork();
    if (pid < 0)
    {
        Logger::logError("Daemon: Unable to fork daemon, code %d (%s)", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // If we got a good PID, then we can exit the parent process.
    if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    // Fork off the parent process: second fork
    pid = fork();
    if (pid < 0)
    {
        Logger::logError("Daemon: Unable to fork daemon, code %d (%s)", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // If we got a good PID, then we can exit the parent process.
    if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    // Change the file mode mask
    umask(0);

    // Open any logs here

    // Create a new SID for the child process
    sid = setsid();
    if (sid < 0)
    {
        Logger::logError("Daemon: Unable to create a new session, code %d (%s)", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Change the current working directory
    if ((chdir("/")) < 0)
    {
        Logger::logError("Daemon: Unable to change directory to %s, code %d (%s)", "/", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Open file descriptors pointing to /dev/null
    // Redirect standard file descriptors to /dev/null
    // Close new file descriptors

    const int new_stdin = open("/dev/null", O_RDONLY);
    if (new_stdin != -1) {
        dup2(new_stdin, STDIN_FILENO);
        close(new_stdin);
    }

    const int new_stdout = open("/dev/null", O_WRONLY);
    if (new_stdout != -1) {
        dup2(new_stdout, STDOUT_FILENO);
        close(new_stdout);
    }

    const int new_stderr = open("/dev/null", O_WRONLY);
    if (new_stderr != -1) {
        dup2(new_stderr, STDERR_FILENO);
        close(new_stderr);
    }
}

void Daemon::parseArgs(const ArgVect & args)
{
    for (ArgVect::const_iterator i(args.begin()); i != args.end(); i++)
    {
        if ((*i) == "--boot-mode" || (*i) == "-b")
        {
            Logger::logInfo("Daemon: Boot mode set.");
            m_bootMode = true;
        }
        else if ((*i) == "--daemon" || (*i) == "-d")
        {
            m_daemon = true;
        }
        else if ((*i) == "--debug")
        {
            Logger::setDebugMode(true);
        }
        else if ((*i) == "--help" || (*i) == "-h")
        {
            usage(EXIT_SUCCESS);
        }
        else if ((*i) == "--quiet" || (*i) == "-q")
        {
            m_quiet = true;
        }
    }
}

// Prints the usage and exits with given status
void Daemon::usage(int status)
{
    printf("\nUsage: %s [options]\n\n"
           "Start the application launcher daemon.\n\n"
           "Options:\n"
           "  -b, --boot-mode  Start %s in the boot mode. This means that\n"
           "                   boosters will not initialize caches and booster\n"
           "                   respawn delay is set to zero.\n"
           "                   The normal mode is restored by sending SIGUSR1\n"
           "                   to the launcher.\n"
           "  -d, --daemon     Run as %s a daemon.\n"
           "  --debug          Enable debug messages and log everything also to stdout.\n"
           "  -q, --quiet      Close fd's 0, 1 and 2.\n"
           "  -h, --help       Print this help.\n\n",
           PROG_NAME_LAUNCHER, PROG_NAME_LAUNCHER, PROG_NAME_LAUNCHER);

    exit(status);
}

int Daemon::sigChldPipeFd() const
{
    return m_sigChldPipeFd[1];
}

int Daemon::sigTermPipeFd() const
{
    return m_sigTermPipeFd[1];
}

int Daemon::sigUsr1PipeFd() const
{
    return m_sigUsr1PipeFd[1];
}

void Daemon::enterNormalMode()
{
    if (m_bootMode)
    {
        m_bootMode = false;

        // Kill current boosters
        killBoosters();

        Logger::logInfo("Daemon: Exited boot mode.");
    }
}

void Daemon::killBoosters()
{
    TypeMap::iterator iter(m_boosterTypeToPid.begin());
    while (iter != m_boosterTypeToPid.end())
    {
        killProcess(iter->second, SIGTERM);
        iter++;
    }

    // NOTE!!: m_boosterTypeToPid must not be cleared
    // in order to automatically start new boosters.
}

Daemon::~Daemon()
{
    delete m_socketManager;
    delete m_singleInstance;
}
