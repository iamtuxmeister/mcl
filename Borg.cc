// We are the Borg

// NOTE: If you don't like those reporting facilities, set borg to 0
// in .mclrc.
// However, I find it nice to see how many people are currently using mcl
// worlwide. mcl does not transmit any sensitive info

#include "mcl.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include "Borg.h"
#include "cui.h"

// abandoned.org
#define IP "216.30.104.62"
#define PORT 4444

// For local testing
//#define IP "127.0.0.1"

void Borg::send(const char *buf) {
    if (fd >= 0)
        write(fd, buf, strlen(buf));
}

// Compose and send out an information packet to the central Consciousness
Borg::Borg() {
    char buf[256];
    fd = socket(AF_INET, SOCK_DGRAM, 0);

    // Fields in the packet are ascii terminated by \n
    // JIC someone runs on a non-Linux-x86 platform (??)
    struct sockaddr_in sock;
    
    sock.sin_family = AF_INET;
    
    // Lookup name here or not?
    // Maybe this should be done asynchronosly instead
    sock.sin_addr.s_addr = inet_addr(IP);
    sock.sin_port = htons(PORT);
    
    if (fd >= 0)
    {
        if (connect(fd, (struct sockaddr*)&sock, sizeof(sock)) < 0)
        {
            close(fd);
            fd = -1;
        }
    }
    
    sprintf(buf, "Version %d\n", VERSION);
    send(buf);
}

int Borg::init_fdset(fd_set *fdset, fd_set*) {
    if (fd >= 0)
        FD_SET(fd,fdset);

    return fd;
}

void Borg::check_fdset(fd_set *fdset, fd_set*) {
    if (fd >= 0 && FD_ISSET(fd,fdset))
    {
        int res;
        char buf[2048];
        char keyword[2048];
        char value[2048];
        
        res = recv(fd, buf, sizeof(buf), 0);
        if (res > 0)
        {
            const char *s;
            int latest = -1;
            char news[2048] = "";
            char stats[2048] = "";
            int users = 0;

            buf[res] = NUL;

//            output->printf("Got: '%s'\n", buf);

            s = one_argument(buf, keyword, false);
            s = one_argument (s, value, false);

            while (value[0])
            {
//                output->printf("Read: %s=%s\n", keyword,value);
                
                if (!strcmp(keyword, "Latest"))
                    latest = atoi(value);
                else if (!strcmp(keyword, "News"))
                    strcpy(news, value);
                else if (!strcmp(keyword, "Users"))
                    users = atoi(value);
                else if (!strcmp(keyword, "Stats"))
                    strcpy(stats, value);
                
                s = one_argument(s, keyword, false);
                s = one_argument (s, value, false);
            }

            if (latest > VERSION)
                messageBox(0, CENTER "Newest version of mcl is %s!\n"
                           CENTER "Go to http://www.andreasen.org/mcl/ and upgrade!\n \n"
                           "News: %s\n", versionToString(latest), news);

            if (config->getOption(opt_borg_verbose))
            {
                if (users)
                    output->printf ("There are %d people using mcl 0.42+ in the world right now.\n", users);
                if (stats[0])
                    output->printf ("Some mcl stats: %s\n", stats);
            }
        }
    }
}

Borg::~Borg() {
    char buf[256];
    sprintf(buf, "TimeUsed %d\nBytesSent %d\nBytesRead %d\nCompRead %lu\nUncompRead %lu\n",
            (int)(current_time - globalStats.starting_time),
            globalStats.bytes_written,
            globalStats.bytes_read,
            globalStats.comp_read,
            globalStats.uncomp_read
           );
    send(buf);
}

