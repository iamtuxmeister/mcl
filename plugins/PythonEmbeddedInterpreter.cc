#include <Python.h>
#include <compile.h>
#include <eval.h>
#include "mcl.h"
#include "Pipe.h"
#include "Interpreter.h"
#include "PythonEmbeddedInterpreter.h"

// Exported functions
extern "C" EmbeddedInterpreter *createInterpreter() {
    return new PythonEmbeddedInterpreter();
}

extern "C" const char* initFunction(const char*) {
    return NULL;
}

extern "C" const char* versionFunction() {
    return "Python embedded interprter";
}


PythonEmbeddedInterpreter::PythonEmbeddedInterpreter()
{
  PyObject *module;

  Py_Initialize();
  module = PyImport_AddModule("__main__");
  globals = PyModule_GetDict(module);
  Py_INCREF(globals);
 
  set("default_var", "");
  
  // given a pattern (default_var) this code replaces all perl $1, $2, $3,
  // etc values with the python equivalent so people don't need to change
  // their regexps.. and for simplicity
  // this is called from match_prepare and substitute_prepare before
  // compiling the user's pattern
  regexp_fixer = code_compile("tmp_matches = re.findall(\"(?:^|\\\\s)\\\\$\\\\d\", default_var)\n"
                              "for x in tmp_matches:\n"
                              "  if x[0] == \" \":\n"
                              "    x = x[2:]\n"
                              "  else:\n"
                              "    x = x[1:]\n"
                              "  default_var = re.sub(\"^\\\\$\" + x, \"\\\" + tmp_match.group(\" + x + \") + \\\"\", default_var)\n"
                              "  default_var = re.sub(\"\\\\s\\\\$\" + x, \" \\\" + tmp_match.group(\" + x + \") + \\\"\", default_var)\n" 
                              "tmp_matches = re.findall(\"\\\\\\\\\\$\\\\d\", default_var)\n"
                              "for x in tmp_matches:\n"
                              "  x = x[2:]\n"
                              "  default_var = re.sub(\"\\\\\\\\\\$\" + x, \"$\" + x, default_var)\n");
}

PythonEmbeddedInterpreter::~PythonEmbeddedInterpreter()
{
  Py_DECREF(globals);
  Py_Finalize();
}

bool PythonEmbeddedInterpreter::load_file(const char *file, bool suppress)
{
  FILE *script = NULL;
  PyObject *obj;
  const char *fullname = findFile(file, ".py");

  if (!(fullname = findFile(file, ".py")) || (!(script = fopen(fullname, "r")))) {
      if (config->getOption(opt_interpdebug) && !suppress)
          report("@@ Loading %s = %m", file);
      return false;
  }
        
  if(!(obj = PyRun_File(script, (char*)fullname, Py_file_input, globals, globals))) {
    if(config->getOption(opt_interpdebug) && !suppress) PyErr_Print();
    fclose(script);
    return false;
  }
  Py_DECREF(obj);
  fclose(script);

  return true;
}

void PythonEmbeddedInterpreter::eval(const char *expression, char *result)
{
  PyObject *obj;

  if(result) *result = '\0';
  if(expression && !*expression) return;
  if(!(obj = PyRun_String((char*)expression, Py_file_input, globals, globals)))
    PyErr_Print();
  else Py_DECREF(obj);
}

bool PythonEmbeddedInterpreter::run(const char *function, const char *args,
                                    char *result)
{
  PyObject *func = get_function(function);
  PyObject *func_args, *res;
  char *str;

  set("default_var", args);

  if (!isEnabled(function))
      return false;

  if(!func) {
    char str[strlen(function)+4];
    sprintf(str, "%s.py", function);
    if(!load_file(str) && !(func = get_function(function))) {
        report("@@ Could not find function '%s' anywhere", function);
        disable_function(function);
        return false;
    }
  }

  func_args = Py_BuildValue("()");
  if(!func_args) return false;
  res = PyEval_CallObject(func, func_args);
  if(!res) {
    PyErr_Print();
    return false;
  }
  Py_DECREF(func_args);
  Py_DECREF(res);

  if(result) {
    str = get_string("default_var");
    strcpy(result, str);
  }
  return true;
}

bool PythonEmbeddedInterpreter::run_quietly(const char *file, const char *args,
                                          char *result, bool suppress)
{
  char *func = strrchr(file, '/');
  char buf[256];

  if(func) func++;
  else func = (char*)file;

  if(!(get_function(func))) {
    sprintf(buf, "%s.py", file);
    if(!load_file(buf, suppress)) {
        disable_function(func);
        return false;
    }
  }

  return run(func, args, result);
}

void *PythonEmbeddedInterpreter::match_prepare(const char *pattern,
                                               const char *commands)
{
  char buf[2048];
  char *tmp;

  match(regexp_fixer, commands, tmp);
  sprintf(buf, "tmp_re = re.compile(r\"%s\")\n"
               "tmp_match = tmp_re.search(default_var)\n"
               "if tmp_match:\n"
               "  default_var = \"%s\"\n"
               "else:\n"
               "  default_var = \"\"\n", pattern, tmp);

  return code_compile(buf);
}

void *PythonEmbeddedInterpreter::substitute_prepare(const char *pattern,
                                                    const char *replacement)
{
  char buf[2048];
  char *tmp;
  
  match(regexp_fixer, replacement, tmp);
  sprintf(buf, "tmp_re = re.compile(r\"%s\")\n"
               "tmp_match = tmp_re.search(default_var)\n"
               "if tmp_match:\n"
               "  default_var = tmp_re.sub(\"%s\", default_var)\n" 
               "else:\n"
               "  default_var = \"\"\n", pattern, tmp);

  return code_compile(buf);
}

bool PythonEmbeddedInterpreter::match(void *code, const char *match_str,
                                      char *&result)
{
  char *str;

  set("default_var", match_str);
  
  if(!(PyEval_EvalCode((PyCodeObject*)code, globals, globals))) {
    report("@@ Error evaluating match or substitute code:\n");
    PyErr_Print();
    return false;
  }
  else {
    str = get_string("default_var");
    result = str;
  
    return (*str) ? true : false;
  }
}

void PythonEmbeddedInterpreter::set(const char *name, int value)
{
  PyObject *obj = Py_BuildValue("i", value);

  if(!obj) {
    PyErr_Print();
    return;
  }
  PyDict_SetItemString(globals, (char*)name, obj);
  Py_DECREF(obj);
}

void PythonEmbeddedInterpreter::set(const char *name, const char *value)
{
  PyObject *obj = Py_BuildValue("s", value);

  if(!obj) {
    PyErr_Print();
    return;
  }
  PyDict_SetItemString(globals, (char*)name, obj);
  Py_DECREF(obj);
}

int PythonEmbeddedInterpreter::get_int(const char *name)
{
  PyObject *obj = PyDict_GetItemString(globals, (char*)name);
  int i;

  if(!obj) {
    PyErr_Print(); 
    return 0;
  }
  PyArg_Parse(obj, "i", &i);
  return i;
}

char *PythonEmbeddedInterpreter::get_string(const char *name)
{
  PyObject *obj = PyDict_GetItemString(globals, (char*)name);
  char *str;

  if(!obj) {
    PyErr_Print();
    return 0;
  }
  PyArg_Parse(obj, "s", &str);
  return str;
}

PyObject *PythonEmbeddedInterpreter::get_function(const char *name)
{
  return PyDict_GetItemString(globals, (char*)name);
}

PyObject *PythonEmbeddedInterpreter::code_compile(const char *code)
{
  PyObject *code_obj;

  if(!(code_obj = Py_CompileString((char*)code, "<string>", Py_file_input)))
    PyErr_Print();
  return code_obj;
}
