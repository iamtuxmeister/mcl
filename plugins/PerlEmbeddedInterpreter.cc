#include <EXTERN.h>
#include "mcl.h"
#include <perl.h>
#include <unistd.h>
#include "PerlEmbeddedInterpreter.h"
#include "Pipe.h"
#include "Interpreter.h"

// Exported functions
extern "C" EmbeddedInterpreter *createInterpreter() {
    return new PerlEmbeddedInterpreter();
}

extern "C" const char* initFunction(const char *) {
    return NULL;
}

extern "C" const char* versionFunction() {
    return "Perl embedded interprter";
}



/* We have to init DynaLoader */

extern "C" {
   void boot_DynaLoader (pTHX_ CV* cv);
//   void boot_Socket (pTHX_ CV* cv);
}

#define my_perl perl_interp

static void xs_init(pTHX)
{
    char *file = __FILE__;
    /* DynaLoader is a special case */
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
//    newXS("Socket::bootstrap", boot_Socket, file);
}

// Initialize the Perl Interpreter
PerlEmbeddedInterpreter::PerlEmbeddedInterpreter() {
    perl_interp = perl_alloc();
    perl_construct((PerlInterpreter*)perl_interp);
    
    char *args[] = {"mclInternalPerlInterpreter", "-w", "-e", ""};
    if ((perl_parse(perl_interp, xs_init, 4, args, __environ)))
        error ("perl_parse error - exiting");

    default_var = perl_get_sv("_", TRUE);
}

PerlEmbeddedInterpreter::~PerlEmbeddedInterpreter() {
    perl_destruct(perl_interp);
    perl_free(perl_interp);
}

// Load up and evaluate a file
bool PerlEmbeddedInterpreter::load_file(const char *filename, bool suppress_error) {
    char *s;
    struct stat stat_buf;
    char rbuf[1024];
    FILE *fp = NULL; // hmm
    const char *fullname;

    sprintf(rbuf, "@@ Loading %s =", filename);

    if (!(fullname = findFile(filename, ".pl")) || (!(fp = fopen(fullname, "r")))) {
        if (config->getOption(opt_interpdebug) && !suppress_error)
            report ("%s %m", rbuf);
        return false;
    }


    if (fstat(fileno(fp), &stat_buf) < 0) {
        fclose(fp);
        if (config->getOption(opt_interpdebug) && !suppress_error)
            report("%s %m (stat)", rbuf);
        return false;
    }
    
    s = new char[stat_buf.st_size + 1];
    fread(s, stat_buf.st_size, 1, fp);
    s[stat_buf.st_size] = NUL;
    fclose (fp);
    
    {
        dSP;
        PUSHMARK(SP);
        perl_eval_pv(s, FALSE);
        if (SvTRUE(ERRSV)) {
            report("%s error:\n%s", rbuf, SvPV(ERRSV, PL_na));
            delete[] s;
            return false;
        }
    }
    
    delete[] s;
    
    if (config->getOption(opt_interpdebug) && !suppress_error)
        report("%s ok", rbuf);
    return true;
}

// Run a function, but do not complain if it doesn't exist
// Give up after having tried to load it once

bool PerlEmbeddedInterpreter::run_quietly(const char *path, const char *arg, char *out, bool suppress_error) {
    // If sys/idle is specified, function=idle but path=sys/idle
    const char *function = strrchr(path, '/');
    if (function)
        function = function+1;
    else
        function = path;

    if (!isEnabled(function))
        return false;

    CV *cv;
    if (!(cv = perl_get_cv((char*) function, FALSE))) {
        char buf[256];
        sprintf(buf, "%s.pl", path);
        
        if (!load_file(buf, suppress_error)) {
            disable_function(function);
            return false;
        }
        
    } else
        ; // free it or something? Dunno.
        
        
    return run(function, arg, out);
}

// Run a function. Set $_ = arg. If out is non-NULL, copy $_ back there
// when done. return false if parse error ocurred or if function doesn't exist
bool PerlEmbeddedInterpreter::run(const char *function, const char *arg, char *out) {
    sv_setpv(default_var, arg);
    
    CV* cv = perl_get_cv((char*)function,FALSE);
    if (!cv) {
        // Try loading it
        if (!load_file(function)) {
            report("@@ Could not find function '%s' anywhere", function);
            return false;
        }
    }
    dSP;
    PUSHMARK(SP);
    
    
    perl_call_pv((char*)function, G_EVAL|G_VOID|G_NOARGS|G_DISCARD);
    if (SvTRUE(ERRSV)) {
        report("@@ Error evaluating function %s: %s",
               function, SvPV(ERRSV, PL_na));
        return false;
    } else {
        if (out) {
            char *s = SvPV(default_var, PL_na);
            strcpy(out,s);
        }

        return true;
    }
}

// Find pattern in str. then take commands and evaluate them, interpoloating variables
// returns a pointer to an anonym subroutine that does this, does not do
// anything itself!
void* PerlEmbeddedInterpreter::match_prepare(const char *pattern, const char *commands) {
    char buf[2048];
    sprintf(buf, "sub { if (/%s/) { $_ = \"%s\";} else { $_ = \"\";} };",  pattern, commands);

//    report("@@ perl_match_prepare %s", buf);
    dSP;
    PUSHMARK(SP);
    return perl_eval_pv(buf, TRUE);
}

// Actually execute the match. Actually is like perl_run except it takes
// a SV* as first parameter *and* it returns 0 if the match fails
bool PerlEmbeddedInterpreter::match(void *perlsub, const char *str, char *&out) {
    sv_setpv(default_var, str);
    
//    report("@@ perl_match(%s)", str);
    dSP;
    PUSHMARK(SP);
    
    perl_call_sv((SV*)perlsub, G_EVAL|G_VOID|G_NOARGS|G_DISCARD);
    if (SvTRUE(ERRSV)) {
        report("@@ Error evaluating autocreated function: %s",
               SvPV(ERRSV, PL_na));
        return false;
    }
    else {
        char *s = SvPV(default_var, PL_na);
        out = s;

        if (!*s)
            return false;
        else
            return true;
    }
}

// As perl_match_prepare except for substitutes
void* PerlEmbeddedInterpreter::substitute_prepare(const char *pattern, const char *replacement) {
    char buf[2048];
    sprintf(buf, "sub { unless (s/%s/%s/g) { $_ = \"\";} };",  pattern, replacement);

//    report("@@ perl_match_prepare %s", buf);
    dSP;
    PUSHMARK(SP);
    return perl_eval_pv(buf, TRUE);
}

// Evalute some loose perl code, put the result in out if non-NULL
void PerlEmbeddedInterpreter::eval(const char *expr, char* out) {
    dSP;
    PUSHMARK(SP);
    SV* res = perl_eval_pv((char*)expr, FALSE);
    
    if (out) {
        if (SvTRUE(ERRSV)) {
            report("@@ Error evaluating %s: %s", expr,
                   SvPV(ERRSV, PL_na));
            *out = NUL;
        }
        else {
            char *s = SvPV(res, PL_na);
            strcpy(out,s);
        }
    }

    //SvREFCNT_dec(res);
}

// Set a named global perl variable to this value
void PerlEmbeddedInterpreter::set(const char *var, int value) {
    SV *v = perl_get_sv((char*)var, TRUE);
    sv_setiv(v,value);
}

void PerlEmbeddedInterpreter::set(const char *var, const char* value) {
    SV *v = perl_get_sv((char*)var, TRUE);
    sv_setpv(v,value);
}

int PerlEmbeddedInterpreter::get_int(const char *name) {
    SV *v = perl_get_sv((char*)name, TRUE);
    return SvIV(v);
}
    
char *PerlEmbeddedInterpreter::get_string(const char *name)
{
  SV *v = perl_get_sv((char*)name, TRUE);

  return SvPV(v, PL_na);
}
