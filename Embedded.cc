#include "mcl.h"
#include "EmbeddedInterpreter.h"
#include "NullEmbeddedInterpreter.h"
#include "Plugin.h"
#include <dlfcn.h>
#include <unistd.h>

// Default interpreter if we don't manage to load anything
static NullEmbeddedInterpreter nullEmbeddedInterpreter;
EmbeddedInterpreter *embed_interp = &nullEmbeddedInterpreter;
static StackedInterpreter *stacked_interp;
static int interp_count = 0; // How many interpreters total do we have

void EmbeddedInterpreter::runCallouts() {
    static int last_callout_time;
    
    if(last_callout_time != current_time) {
        last_callout_time = current_time;
        embed_interp->run_quietly("sys/idle", "", 0);
    }
}

StackedInterpreter::StackedInterpreter(EmbeddedInterpreter *e1,EmbeddedInterpreter *e2) {
    interpreters.insert(e1);
    interpreters.insert(e2);
}

StackedInterpreter::~StackedInterpreter() {
    FOREACH(EmbeddedInterpreter*, e, interpreters)
        delete e;
}

// Slight code duplication here, but can't do much about it since the functions called are different,
// unless I start messing around with some functors I guess
bool StackedInterpreter::run(const char *function, const char *arg, char *out) {
    char buf[interpreters.count()][MAX_MUD_BUF];
    int i = 0;
    bool res = false;
    
    FOREACH(EmbeddedInterpreter*, e, interpreters) {
        res = e->run(function, arg, buf[i]) || res;
        arg = buf[i++];
    }

    if (out && res)
        strcpy(out, buf[i-1]);
    return res;
}

bool StackedInterpreter::load_file(const char* filename, bool suppress) {
    bool res = false;
    FOREACH(EmbeddedInterpreter*, e, interpreters)
        res = e->load_file(filename, suppress) || res;
    return res;
}

void StackedInterpreter::eval(const char* expr, char* out) {
    interpreters[0]->eval(expr, out);
}

void *StackedInterpreter::match_prepare(const char* pattern, const char* replacement) {
    return interpreters[0]->match_prepare(pattern,replacement);
}

void* StackedInterpreter::substitute_prepare(const char* pattern, const char* replacement)  {
    return interpreters[0]->match_prepare(pattern,replacement);
}

bool StackedInterpreter::match(void *perlsub, const char *str, char *&out) {
    return interpreters[0]->match(perlsub, str, out);
}

void StackedInterpreter::set(const char *var, int value) {
    FOREACH(EmbeddedInterpreter*, e, interpreters)
        e->set(var, value);
}

void StackedInterpreter::set(const char *var, const char* value) {
    FOREACH(EmbeddedInterpreter*, e, interpreters)
        e->set(var, value);
}

int StackedInterpreter::get_int(const char* name) {
    return interpreters[0]->get_int(name);
}
char* StackedInterpreter::get_string(const char*name) {
    return interpreters[0]->get_string(name);
}



bool StackedInterpreter::run_quietly(const char* path, const char *arg, char *out, bool suppress_error) {
    char buf[interpreters.count()][MAX_MUD_BUF];
    int i = 0;
    bool res = false;
    
    FOREACH(EmbeddedInterpreter*, e, interpreters) {
        res = e->run_quietly(path, arg, buf[i], suppress_error) || res;
        arg = buf[i++];
    }

    if (out && res)
        strcpy(out, buf[i-1]);

    return res;
}


List<Plugin*> Plugin::plugins;

Plugin::Plugin(const char *_filename, void *_handle) : filename(_filename), handle(_handle) {
}

Plugin::~Plugin() {
    dlclose(handle);
}

// Unload all the scripts
void Plugin::done() {
    FOREACH(Plugin*,p,plugins)
        if (p->doneFunction) {
            p->doneFunction();
            delete p;
        }
}

// Load shared object and update interpreter settings
Plugin * Plugin::loadPlugin(const char *filename, const char *args) {
    char buf[1024];
    void *handle;
    
    // Try global path or home directory
    snprintf(buf, sizeof(buf), "%s/.mcl/plugins/%s", getenv("HOME"), filename);
    if (access(buf, R_OK) < 0) {
        snprintf(buf, sizeof(buf), "%s/plugins/%s", MCL_LOCAL_LIBRARY_PATH, filename);
        if (access(buf, R_OK) < 0) {
            snprintf(buf, sizeof(buf), "%s/plugins/%s", MCL_LIBRARY_PATH, filename);
            if (access(buf, R_OK) < 0)
                error ("Error loading %s: not found\n"
                       "I tried looking for and failed to find:\n"
                       "  %s/.mcl/plugins/%s\n"
                       "  %s/plugins/%s\n"
                       "  %s/plugins/%s\n"
                       "If you installed the standard binary distribution, you probably\n"
                       "forgot to move the plugin files to one of the above places.\n"
                       , filename, getenv("HOME"), filename, MCL_LOCAL_LIBRARY_PATH, filename, MCL_LIBRARY_PATH, filename);
        }
    }
    
    if (!(handle = dlopen(buf, RTLD_LAZY|RTLD_GLOBAL)))
        error ("Error loading %s: %s\n", buf, dlerror());
    
    Plugin *plugin = new Plugin(buf, handle);
    plugins.insert(plugin);
    
    // Call the initFunction, it returns an error message in case of error
    const char *load_result;
    
    if (!(plugin->initFunction = (InitFunction*)dlsym(handle, "initFunction")))
        error ("Error loading %s: modules does not have a initFunction\n", buf);
    
    if ((load_result = plugin->initFunction(args)))
        error ("Error loading %s: %s\n", buf, load_result);
    
    // Display some information about the module if the module so wishes
    plugin->versionFunction = (VersionFunction*)dlsym(handle, "versionFunction");
    plugin->doneFunction = (DoneFunction*)dlsym(handle, "doneFunction");
    
    // Now let's setup the hooks
    if ((plugin->createInterpreterFunction = (CreateInterpreterFunction*)dlsym(handle, "createInterpreter"))) {
        EmbeddedInterpreter *interp = plugin->createInterpreterFunction();
        if (!interp)
            error ("Module %s: error creating interpreter", buf);
        else {
            if (stacked_interp) // If we already have a stacked set of interpreters, add it there
                stacked_interp->add(interp);
            else if (interp_count == 1) {
                stacked_interp = new StackedInterpreter(embed_interp, interp);
                embed_interp = stacked_interp;
            } else // this is the first interpreter
                embed_interp = interp;
        }
        interp_count++;
    }
    
    return plugin;
}

const char* Plugin::getVersionInformation() {
    if (versionFunction)
        return versionFunction();
    else
        return NULL;
}

void Plugin::displayLoadedPlugins() {
    if (plugins.count() == 0)
        report("Modules loaded: none");
    else {
        report("Modules loaded: ");
        FOREACH(Plugin*, p, plugins) {
            const char *info = p->getVersionInformation();
            report("%s: %s", ~p->filename, info ? info : "(no version information)");
        }
    }
}

// Load all plugins
void Plugin::loadPlugins(const char *plugins) {
    char module_name[INPUT_SIZE];
    char module_args[INPUT_SIZE];
    const char *s = plugins;
    char *out;

    // module_name [args], module_name [args]...
    for (;*s;) {
        while (isspace(*s))
            s++;

        out = module_name;
        while (*s && !isspace(*s) && *s != ',')
            *out++ = *s++;
        *out = NUL;
        if (!module_name[0])
            continue;

        out = module_args;
        if (*s == ' ') { // module args follow space
            out++;
            while(*s && *s != ',')
                *out++ = *s++;
        }
        *out = NUL;
        if (*s == ',')
            s++;

        loadPlugin(Sprintf("%s.so", module_name), module_args);
    }
}


bool NullEmbeddedInterpreter::load_file(const char*, bool) {
    return false;
}

bool NullEmbeddedInterpreter::run_quietly(const char*, const char*, char*,bool) {
    return false;
}

void EmbeddedInterpreter::enable_function(const char *function) {
    for (String *s = failed.rewind(); s; s = failed.next())
        if (*s == function) {
            failed.remove(s);
            delete s;
        }
}

void EmbeddedInterpreter::disable_function(const char *function) {
    String *s = new String(function);
    failed.append(s);
}

bool EmbeddedInterpreter::isEnabled(const char *function) {
    for (String *s = failed.rewind(); s; s = failed.next())
        if (*s == function)
            return false;
    return true;
}


const char *EmbeddedInterpreter::findFile(const char *filename, const char *suffix) {
    // Add suffix if not there already
    if (strlen(filename) < strlen(suffix) || strcmp(filename+strlen(filename)-strlen(suffix), suffix))
        filename = Sprintf("%s%s", filename, suffix);
    
    const char *full;
    
    // Try first compared to the .mcl directory
    if (filename[0] != '/') {
        full = Sprintf("%s/.mcl/%s", getenv("HOME"), filename);
        if (access(full, R_OK) == 0)
            return full;

        // Globally installed files
        full = Sprintf("/usr/lib/mcl/%s", filename);
        if (access(full, R_OK) == 0)
            return full;

        full = Sprintf("/usr/local/lib/mcl/%s", filename);
        if (access(full, R_OK) == 0)
            return full;
    }
    
    if (access(filename, R_OK) == 0)
        return filename;
    
    return NULL;
}

