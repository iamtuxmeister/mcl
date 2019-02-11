
#include "mcl.h"
#include <sys/time.h>
#include <unistd.h>

List<Selectable*> Selectable::ioList;

// Do a select on all registered Selectable objects
void Selectable::select(int sec, int usec)
{
    fd_set in_set, out_set;
    Selectable *i;
    int max_fd = 0;
    struct timeval tv;


    tv.tv_usec = usec;
    tv.tv_sec = sec;
    
    FD_ZERO(&in_set);
    FD_ZERO(&out_set);
    
    for (i = ioList.rewind(); i;  i = ioList.next())
        max_fd = max(max_fd, i->init_fdset(&in_set, &out_set));
    
    
    while (::select(max_fd+1, &in_set, &out_set, NULL, &tv) < 0)
        if (errno != EAGAIN && errno != EINTR)
        {
            perror ("select");
            exit (1);
        }

    // @@ Async connections are ready when ready to be written to, not read from
    for (i = ioList.rewind(); i;  i = ioList.next())
        i->check_fdset(&in_set, &out_set);
}

Selectable::Selectable() {
    ioList.append(this);
}

Selectable::~Selectable() {
    ioList.remove(this);
}


