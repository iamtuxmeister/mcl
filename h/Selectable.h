// Selectable objects are objects that need to select() on
// some FDs

class Selectable {
public:
    virtual int init_fdset(fd_set *readset, fd_set *writeset) = 0;
    virtual void check_fdset(fd_set *readset, fd_set *writeset) = 0;
    Selectable();
    virtual ~Selectable();

    static void select(int, int);

private:
    static List<Selectable*> ioList;
};
