#ifndef PYTHONEMBEDDEDINTERPRETER_H_
#define PYTHONEMBEDDEDINTERPRETER_H_

#include "EmbeddedInterpreter.h"
#include "Python.h"

class PythonEmbeddedInterpreter : public EmbeddedInterpreter
{
  public:
    ~PythonEmbeddedInterpreter();
    virtual bool load_file(const char*, bool suppress = false);
    virtual void eval(const char*, char*);
    virtual bool run(const char*, const char*, char*);
    virtual bool run_quietly(const char*, const char*, char*,
                             bool suppress = true);
    virtual void *match_prepare(const char*, const char*);
    virtual void *substitute_prepare(const char*, const char*);
    virtual bool match(void*, const char*, char*&);
    virtual void set(const char*, int);
    virtual void set(const char*, const char*);
    virtual int get_int(const char*);
    virtual char *get_string(const char*);
  
    PythonEmbeddedInterpreter();
    PyObject *get_function(const char*);
    PyObject *code_compile(const char*);
    
    PyObject *globals;
    PyObject *regexp_fixer;
};  
#endif
