#include "mcl.h"
#include "Pipe.h"
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "cui.h"
#include "Interpreter.h"

Pipe::Pipe(int fd1, int fd2) {
    // socketpair has apparently greater 'capacity' than pipe
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
        error ("Pipe::Pipe socketpair: %m");
    
    if (fd1 != -1) {
        if (dup2(fds[Read], fd1) < 0)
            error ("Pipe::Pipe dup2 (%d->%d) %m", fds[Read], fd1);
        close(fds[Read]);
        fds[Read] = fd1;
    }

    if (fd2 != -1) {
        if (dup2(fds[Write], fd2) < 0)
            error ("Pipe::Pipe dup2 (%d->%d) %m", fds[Write], fd2);
        close(fds[Write]);
        fds[Write] = fd2;
    }
}

Pipe::~Pipe() {
    close(fds[0]);
    close(fds[1]);
}

int Pipe::read (char *buf, int count) {
    count = ::read(fds[Read], buf, count);
    return count;
}

int Pipe::write(const char *buf, int count) {
    count = ::write(fds[Write], buf, count);
    return count;
}

int Pipe::init_fdset(fd_set *set, fd_set*) {
    FD_SET(fds[Read], set);
    return fds[Read];
}

void Pipe::check_fdset(fd_set *set, fd_set*) {
    if (FD_ISSET(fds[Read], set))
        inputReady();
}

InterpreterPipe::InterpreterPipe() : Pipe(), pos(0) {
}

void InterpreterPipe::inputReady() {
    int res;
    char *s;
    
    res = read(line_buf+pos, sizeof(line_buf)-pos);
    if (res <= 0)
        error ("inputReady::read:%m");
    
    pos += res;
    
    while ((s = (char*)memchr(line_buf, '\n', pos))) {
        char buf[PIPE_BUF];
        int len = s-line_buf;
        
        memcpy(buf, line_buf, len);
        buf[len] = NUL;
        memmove(line_buf, s+1, pos - len);
        pos -= len+1;
        interpreter.add(buf, EXPAND_ALL);
    }
}

OutputPipe::OutputPipe() : Pipe(-1, STDOUT_FILENO) {
}

void OutputPipe::inputReady() {
    char buf[PIPE_BUF];
    int count = read(buf, sizeof(buf));
    if (count < 0)
        error ("OutputPipe::inputReady:%m");
    
    buf[count] = NUL;
    output->printf("%s", buf);
}

int Pipe::getFile(End e) {
    return (fds[e]);
}


