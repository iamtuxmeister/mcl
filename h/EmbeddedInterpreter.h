#ifndef EMBEDDEDINTERPRETER_H_
#define EMBEDDEDINTERPRETER_H_

#include <stdio.h>
#include "List.h"

bool load_shared_object(const char *filename);
extern List<String*> modules_loaded;

class EmbeddedInterpreter
{
  public:
    virtual ~EmbeddedInterpreter() {}
    virtual bool load_file(const char*, bool suppress = false) = 0;
    virtual void eval(const char*, char*) = 0;
    virtual bool run(const char*, const char*, char*) = 0; 
    virtual bool run_quietly(const char*, const char*, char*,
                              bool suppres = true) = 0;
    virtual void *match_prepare(const char*, const char*) = 0;
    virtual void *substitute_prepare(const char*, const char*) = 0;
    virtual bool match(void*, const char*, char*&) = 0;
    virtual void set(const char*, int) = 0;
    virtual void set(const char*, const char*) = 0;
    virtual int get_int(const char*) = 0;
    virtual char *get_string(const char*) = 0;

    void enable_function(const char*);
    void disable_function(const char*);
    bool isEnabled(const char *);

    virtual bool isStacked() { return false; }

    static void runCallouts();

protected:
    const char *findFile(const char *fname, const char *suffix); // given e.g. "foobar" and a suffix, search through the script paths

private:
    List<String*> failed;
};

class StackedInterpreter : public EmbeddedInterpreter {
public:
    StackedInterpreter(EmbeddedInterpreter *i1, EmbeddedInterpreter *i2);
    ~StackedInterpreter();
    void add(EmbeddedInterpreter *e) { interpreters.insert(e); }

    virtual bool load_file(const char*, bool suppress = false);
    virtual void eval(const char*, char*);
    virtual bool run(const char*, const char*, char*);
    virtual bool run_quietly(const char*, const char*, char*, bool suppres = true);
    virtual void *match_prepare(const char*, const char*);
    virtual void *substitute_prepare(const char*, const char*) ;
    virtual bool match(void*, const char*, char*&) ;
    virtual void set(const char*, int);
    virtual void set(const char*, const char*);
    virtual int get_int(const char*);
    virtual char *get_string(const char*);
        
    virtual bool isStacked() { return true; }
private:
    List<EmbeddedInterpreter*> interpreters;

};

#endif
