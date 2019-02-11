

class Borg : public Selectable
{
public:
    Borg();
    ~Borg();
    void send(const char *);
    void check_fdset(fd_set *fd, fd_set*);
    int init_fdset(fd_set *fd, fd_set*);
    
private:
    int fd;
};

