// a class to keep a list of commands and execute them one at a time

#include "mcl.h"
#include "cui.h"
#include "Option.h"
#include "MessageWindow.h"
#include "Interpreter.h"
#include "Shell.h"
#include "Action.h"
#include "Alias.h"
#include "Chat.h"
#include "Session.h"
#include <time.h>

Interpreter interpreter;
extern MUD *lastMud;

bool actions_disabled, aliases_disabled, macros_disabled;
// Pick off one argument, optionally smashing case and place it in buf
// Eat whitespace, respect "" and ''
const char *one_argument (const char *argument, char *buf, bool smash_case) {
    char end;
    
    while(isspace(*argument))
        argument++;

    if (*argument == '\'' || *argument == '\"')
        end = *argument++;
    else
        end = NUL;

    while(*argument && (end ? *argument != end : !isspace(*argument)))  {
        *buf++ = smash_case ? tolower(*argument) : *argument;
        argument++;
    }

    *buf++ = NUL;

    if (*argument && end)
        argument++;

    while(isspace(*argument))
        argument++;

    return argument;
}


void Interpreter::execute() {
    int count = 0;
    
    while(commands.count())
    {
        String line = *commands[0];
        commands.remove(commands[0]);
        
        if (++count > 100)
        {
            output->printf ("Recursing alias? Next command would be \"%s\".\n", (const char*)line);
            while(commands.count())
                commands.remove(commands[0]);
            break;
        }

        char out_buf[MAX_INPUT_BUF];
        const char *t = out_buf;

        if (!(embed_interp->run_quietly("sys/send", line, out_buf)))
            t = line;
        
        if (t[0] == commandCharacter)
            mclCommand (t+ 1);
        else if (currentSession) {
            currentSession->writeMUD(t);
        }
        else
            status->setf ("You are not connected. Use Alt-O to connect to a MUD.");
    }
}

// Legal speedwalk strings
const char legal_standard_speedwalk[] = "nsewud";
const char legal_extended_speedwalk[] = "nsewudhjkl";

#define MAX_SPEEDWALK_REPEAT 99 // just in case someone typos '244n'
#define ADD2(s) *pc2++ = s[0]; *pc2++ = s[1];

// Expand possible speedwalk string
void Interpreter::expandSpeedwalk(const char *s, int flags)
{
    const char *pc;
    bool try_speedwalk = config->getOption(opt_speedwalk);
    const char *legal_speedwalk;
    
    if (s[0] == config->getOption(opt_speedwalk_character)) {
        try_speedwalk = true;
        legal_speedwalk = legal_extended_speedwalk;
        s++;
    } else
        legal_speedwalk = legal_standard_speedwalk;
    
    if (try_speedwalk)
    {
        for (pc = s; *pc; pc++)
            if (!isdigit(*pc) && !strchr (legal_speedwalk, *pc))
                break;
        
        // the harcoded "News" will probably be a config option	
        if (!*pc && strcasecmp(s,"news") && (strchr(legal_speedwalk, *(pc-1)))) // it IS a speedwalk string
        {
            int repeat = 0;
            
            for (pc = s; *pc; pc++)
                if (isdigit(*pc))
                    repeat = 10 * repeat + *pc - '0';
                else
                {
                    repeat = max(1,min(MAX_SPEEDWALK_REPEAT,repeat));
                    char *walk = (char*)alloca(repeat*3+1);
                    
                    char *pc2 = walk;
                    
                    while(repeat--)
                    {
                        switch (*pc) {
                        case 'h':
                            ADD2("nw"); break;
                        case 'j':
                            ADD2("ne"); break;
                        case 'k':
                            ADD2("sw"); break;
                        case 'l':
                            ADD2("se"); break;
                            
                        default:
                            *pc2++ = *pc;
                        }

						*pc2++ = NUL;
						add (walk, EXPAND_NONE);
					}
                    
                    repeat = 0;
                }
            
            return;
        }
    }
    add(s,flags & ~EXPAND_SPEEDWALK);
}

const char* Interpreter::expandVariables(const char *s) {
    if (!strchr(s, '%'))
        return s;
    else {
        
        StaticBuffer buf(MAX_MUD_BUF);
        char *out;
        
        for (out = buf; *s; s++)  {
            struct tm *t = NULL;
            
            if (*s == '%' && *(s+1))
            {
                switch(*++s)
                {
                    // @@ change this to snprintf
                case 'h': // remote hostname
                    out += sprintf(out, "%s", currentSession ? currentSession->mud.getHostname() : "");
                    break;
                    
                case 'p': // remote portname
                    out += sprintf(out, "%d", currentSession ? currentSession->mud.getPort() : 0);
                    break;
                    
                case 'n':
                    out += sprintf(out, "%s", currentSession ?~ currentSession->mud.name : "");
                    break;
                    
                case 'P':
                    out += sprintf(out, "%d", currentSession ? currentSession->getLocalPort(): 0);
                    break;
                    
                case 'f': // mudFTP port
                    out += sprintf(out, "%d", currentSession ? currentSession->mud.getPort() + 6 : 0);
                    break;
                    
                    // strftime uses some slightly different characters
                    
#define TIME(my_char,their_char) case my_char: \
    { \
    char fmt[3] = { '%', their_char, NUL }; \
    if (!t) t= localtime(&current_time); \
    out += strftime(out, (buf+MAX_MUD_BUF)-out, fmt, t); \
    break; \
    }
                    
                    TIME('H', 'H'); // Hour
                    TIME('m', 'M'); // Minute
                    TIME('M', 'b'); // Abbreviated month name
                    TIME('w', 'a'); // Abbreviated weekday name
                    TIME('t', 'c'); // Preferred date and time representation
                    TIME('d', 'd'); // Day of the month
                    TIME('y', 'y'); // Year (last two digits)
                    TIME('Y', 'Y'); // Full year
                    TIME('o', 'm'); // month number, 01-11
                    
                    
                case '%': // Just one %
                    out += sprintf(out, "%%");
                    break;
                    
                    // Need something like username/password here
                    
                default:
                    *out++ = *s;
                }
            }
            else
                *out++ = *s;
            
        }
        
        *out = NUL;
        return buf;
    }
}

// %variables
// speedwalk (can not recurse)
// aliases (can recurse)
// ;
// Results from aliases: 2,3
// Command

// Expand stuff in this line before it gets sent to the MUD
void Interpreter::add(const char *s, int flags, bool back) {
    if (s[0] == config->getOption(opt_escape_character)) { // short circuit
        if (back)
            commands.insert(new String(s+1));
        else
            commands.append(new String(s+1));
        return;

    }

    if (flags & EXPAND_VARIABLES) {
        s = expandVariables(s);
        flags = flags & ~EXPAND_VARIABLES;
    }

    if (flags & EXPAND_ALIASES)
        expandAliases(s, flags);
    else if (flags & EXPAND_SPEEDWALK)
        expandSpeedwalk(s, flags);
    else if (flags & EXPAND_SEMICOLON)
        expandSemicolon(s, flags);
    else if (s[0] == commandCharacter && isdigit(s[1])) {
        char count[MAX_MUD_BUF];
        s = one_argument(s+1, count, false);
        int n = atoi(count);
        if (n > 0 && n < 100) {
            while(n-- > 0)
                add(s, EXPAND_ALL);
        } else
            status->setf("Repeat count must be between 0 and 100");
        return;
    } else {
        if (back)
            commands.insert(new String(s));
        else
            commands.append(new String(s));
    }
}

void Interpreter::expandSemicolon(const char *s, int flags) {
    if ((strchr(s, ';'))) {
        char buf[strlen(s)+1];
        char *out;

        bool got_semicolon = false;

        for (;*s;) {
            
            for (out = buf; *s; s++ ) {
                if (*s == '\\' && s[1] == ';') { // escaped semicolon
                    *out++ = ';';
                    s++;
                }  else if (*s == ';') {
                    got_semicolon = true;
                    break;
                }
                else
                    *out++ = *s;
            }

            // Deleting spaces just before the ;
            while (out > buf && isspace(out[-1]))
                out--;
            *out = NUL;

            // Skip the semicolon, if there was one
            if (*s == ';')
                s++;
                

            // Deleting space just after the ; if appropriate
            while(*s && isspace(*s))
                s++;

            if (got_semicolon)
                add(buf, EXPAND_ALL); // unconditionally expand everything so aliases are expanded with their contents
            else // escaped semicolons only
                add(buf, flags & ~ EXPAND_SEMICOLON);
        }
    }
    else
        add(s, flags & ~EXPAND_SEMICOLON);
}

// ought to move some of this stuff to separate functions, this function is growing LARGE
void Interpreter::expandAliases(const char* s, int flags) {
    
    // Isolate alias name
    
    if (!s[0]) // special case, unfortunately
        add("", EXPAND_NONE);
    else {
        int count;
        char name[MAX_ALIAS_BUFFER];
        char buf[MAX_MUD_BUF];
        
        if (embed_interp->run_quietly("sys/command", s, buf)) {
            if (!buf[0]) // command cancelled
                return;
            s = buf;
        }
        
        // Allow e.g. @ or % to be valid alias names without requiring a space
        // afterwards
        if (!isalpha(s[0])) {
            count = 1;
            name[0] = s[0];
            name[1] = NUL;
            count = 1;
        } else
            sscanf(s, "%s %n", name, &count); // skip spaces.. will this break anything?
        
        // Easy way to add a command
        if (embed_interp->run_quietly(Sprintf("cmd_%s", name),s+count, buf, true))
            return; // command executed, don't send this anywhere
        
        Alias *a = NULL;
        
        if (!aliases_disabled)
            a = currentSession ? currentSession->mud.findAlias(name) : globalMUD.findAlias(name);

        if (a) {
            char out_buf[MAX_MUD_BUF];
            a->expand(s+count, out_buf);
            add(out_buf, EXPAND_ALL); // Expand everything again
        }					
        else // no expansion ocurred
            add(s, flags & ~ EXPAND_ALIASES);
    }
}

// Execute a mcl Command
void Interpreter::mclCommand (const char *s)
{
    char cmd[256];
    int w = 80, h=10, x=0, y=3, t=10;
    bool popup = true;
    bool noborder = false;
    int color = bg_black|fg_white;
    
    s = one_argument(s, cmd, true);
    
    if (!strcmp(cmd, "quit"))
        mclFinished = true;
    
    else if (!strcmp(cmd, "echo"))
    {
        output->printf("%s\n", s);
    }
    else if (!strcmp(cmd, "status"))
    {
        status->setf("%s", s);
    }
    else if (!strcmp(cmd, "bell"))
    {
        screen->flash();
    }
    else if (!strcmp(cmd, "exec"))
    {
        int c;
        String res;
        
        OptionParser o(s, "w:h:x:y:t:");
        
        while((c = o.nextOption(res)))
            switch (c)
            {
            case 'w':
                w  = atoi(res);
                break;
                
            case 'h':
                h = atoi(res);
                break;
                
            case 'x':
                x = atoi(res);
                break;
                
            case 'y':
                y = atoi(res);
                break;
                
            case 't':
                t = atoi(res);
                break;
                
            default:
                status->setf("%cexec: %s", CMDCHAR, (const char*)res);
                return;
            }
        
        // Do some sanity checking
        if (h < 3 || w < 3 || x < 0 || y < 0)
            status->setf("Value too low.");
        else if (w+x > screen->width || y+h > screen->height)
            status->setf("Value too large.");
        else
            new Shell(screen, res, w,h,x,y,t);
        
    }
    else if (!strcmp(cmd, "window"))
    {
        int c;
        String logfile;
        String res;
        
        OptionParser o(s, "w:h:x:y:t:Hl:Bc:");
        
        while((c = o.nextOption(res)))
            switch (c)
            {
            case 'w':
                w  = atoi(res);
                break;
                
            case 'h':
                h = atoi(res);
                break;
                
            case 'x':
                x = atoi(res);
                break;
                
            case 'y':
                y = atoi(res);
                break;
                
            case 't':
                t = atoi(res);
                break;
                
            case 'l':
                logfile = res;
                break;
                
            case 'H':
                popup = false;
                break;
            case 'B':
                noborder = true;
                break;
                
            case 'c':
                color = atoi(res);
                break;
                
            default:
                status->setf("%cwindow: %s", CMDCHAR, (const char*)res);
                return;
            }
        
        int min_h = 3;
        if (noborder)
            min_h = 1;
        
        // Do some sanity checking
        if (h < min_h || w < min_h)
            status->setf("%cwindow: Value too low.", CMDCHAR);
        else if (w+abs(x) > screen->width || abs(y)+h > screen->height)
            status->setf("%cwindow: Value too large.", CMDCHAR);
        else if (!res.len())
            status->setf("%cwindow needs a name", CMDCHAR);
        else if (MessageWindow::find(res))
        {
            status->setf("Window %s already exists", (const char*) res);
        }
        else  {
            Window::Style style;
            if (!noborder)
                style = Window::Bordered;
            else
                style = Window::None;
            
            MessageWindow *mw =  new MessageWindow(screen, res, logfile, w,h,x,y, style, t, popup, color);
            
            if (!popup)
                mw->show(false);
        }
        
    }
    
    else if (!strcmp(cmd, "kill")) {
        MessageWindow *w;
        if (!(w = MessageWindow::find(s)))
            status->setf ("No such window: %s", s);
        else
            w->die();
    }
    else if (!strcmp(cmd, "clear")) {
        char name[128];
        
        s = one_argument(s, name, false);
        
        if (!name[0])
            status->setf("%cclear what window?", CMDCHAR);
        else {
            MessageWindow *w = MessageWindow::find(name);
            if (!w)
                status->setf("%cclear: %s: no such window", CMDCHAR, name);
            else
                w->clear();
        }
    }
    else if (!strcmp(cmd, "prompt")) {
        inputLine->set_prompt(s);
    }
    else if (!strcmp(cmd, "print")) {
        char name[128];
        
        s = one_argument(s, name, false);
        
        if (!name[0])
            status->setf("%cprint to what window?", CMDCHAR);
        else
        {
            MessageWindow *w = MessageWindow::find(name);
            if (!w)
                status->setf("%cprint: %s: no such window", CMDCHAR, name);
            else
                w->addInput(s);
        }
    } else if (!strcmp(cmd, "disable")) {
        if (!strcasecmp(s, "aliases")) {
            aliases_disabled = true;
            status->setf("Aliases disabled.");
        }
        else if (!strcasecmp(s, "actions")) {
            actions_disabled = true;
            status->setf("Actions disabled.");
        }
        else if (!strcasecmp(s, "speedwalk")) {
            config->setOption(opt_speedwalk,false);
            status->setf("Speedwalk disabled.");
        }
        else if (!strcasecmp(s, "macros")) {
            macros_disabled = true;
            status->setf("Keyboard macros disabled.");
        }
        else if (!strcasecmp(s, "semicolon")) {
            config->setOption(opt_expand_semicolon, false);
            status->setf("Semicolon NOT expanded on input line");
        }
        else
            status->setf("%cdisable (actions|aliases|speedwalk|macros|semicolon)", CMDCHAR);
    } else if (!strcmp(cmd, "enable")) {
        if (!strcasecmp(s, "aliases")) {
            status->setf("Aliases enabled.");
            aliases_disabled = false;
        }
        else if (!strcasecmp(s, "speedwalk")) {
            config->setOption(opt_speedwalk, true);
            status->setf("Speedwalk enabled.");
        }
        else if (!strcasecmp(s, "actions")) {
            status->setf("Actions enabled.");
            actions_disabled = false;
        }
        else if (!strcasecmp(s, "macros")) {
            macros_disabled = false;
            status->setf("Keyboard macros enabled.");
        }
        else if (!strcasecmp(s, "semicolon")) {
            config->setOption(opt_expand_semicolon, true);
            status->setf("Semicolon expanded on input line.");
        }
        else
            status->setf("%cenable (actions|aliases|speedwalk|macros|semicolon)", CMDCHAR);
    }
    
    else if (!strcmp(cmd, "writeconfig")) {
        status->setf("Saving configuration to %s", s ? s : ~config->filename);
        config->Save(s);
    }
    else if (!strcmp(cmd, "alias")) // add alias
    {
        char name[MAX_INPUT_BUF];
        
        if (!s[0])
            status->setf ("Press Alt-A to list active aliases; Alt-O then A to list local ones");
        else 
        {
            s = one_argument(s, name, false);
            Alias *a = globalMUD.findAlias(name);
            
            if (!s[0]) {
                if (!a)
                    status->setf ("There is no global alias named '%s'", name);
                else  {
                    status->setf ("Removed global alias '%s'", name);
                    globalMUD.alias_list.remove(a);
                    delete a;
                }
            } else {
                if (a) {
                    status->setf ("Replaced alias '%s' with %s", ~a->name, s);
                    a->text = s;
                } else {
                    status->setf ("Added new alias %s = %s", name,s);
                    globalMUD.alias_list.insert(new Alias(name,s));
                }
            }
        }
    }
    else if (!strcmp(cmd, "close")) {
        if (!currentSession || currentSession->state == disconnected)
            status->setf ("You are not connected - nothing to close");
        else
        {
            status->setf ("Closing link to %s@%s:%d", ~currentSession->mud.name, currentSession->mud.getHostname(), currentSession->mud.getPort());
            inputLine->set_default_prompt();
            delete currentSession;
            currentSession = NULL;
        }
    }
    else if (!strcmp(cmd, "open")) {
        if (currentSession && currentSession->state != disconnected)
            status->setf ("You are connected to %s, use %cclose first", ~currentSession->mud.name, CMDCHAR);
        else if (!s[0])
            status->setf ("%copen connection to where?", CMDCHAR);
        else
        {
            MUD *mud;
            if (!(mud = config->findMud (s)))
                status->setf ("MUD '%s' was not found", s);
            else
            {
                delete currentSession;
                currentSession = new Session(*mud, output);
                currentSession->open();
                lastMud = mud;
            }
        }
    }
    
    else if (!strcmp(cmd, "macro")) {
        char name[MAX_INPUT_BUF];
        Macro *m;
        
        s = one_argument(s, name, true);
        int key = key_lookup(name);
        
        if (key < 0)
            status->setf ("Unknown key in macro definition: %s", name);
        else {
            m = globalMUD.findMacro(key, false);
            
            if (!s[0]) { // remove macro
                if (!m)
                    status->setf("No such macro");
                else {
                    globalMUD.macro_list.remove(m);
                    status->setf("Removed macro %s => %s", name, ~m->text);
                    delete m;
                }
            } else { // Add new or change existing
                if (!m) {
                    m = new Macro(key, s);
                    status->setf("Added new macro: %s => %s", name,s);
                    globalMUD.macro_list.append(m);
                } else {
                    status->setf("Changed macro %s: from %s to %s", name, ~m->text, s);
                    m->text = s;
                }
            }
        }
    }
    else if (!strcmp(cmd, "action") || !strcmp(cmd, "subst"))
    {
        char name[256];
        s = one_argument(s, name, false);
        
        if (!name[0])
            status->setf ("Add/remove what action?");
        else if (!s[0]) {
            FOREACH(Action*, a, globalMUD.action_list)
                if (a->pattern == name)
                {
                    status->setf ("Action %s => %s removed", (const char*) a->pattern, (const char *) a->commands);
                    globalMUD.action_list.remove(a);
                    delete a;
                    return;
                }
            
            status->setf("Action '%s' was not found", name);
        } else {
            char buf[MAX_MUD_BUF];
            String res;
            
            sprintf(buf, "\"%s\" %s", name, s);
            
            Action *new_action = Action::parse (buf, res, (!strcmp(cmd, "action")) ? Action::Trigger : Action::Replacement);
            if (!new_action)
                status->setf ("Error compiling action: %s", ~res);
            else {
                FOREACH(Action*,a, globalMUD.action_list)
                    if (a->pattern == name)  {
                        status->setf ("Replaced action '%s' with '%s'", (const char*) new_action->pattern, (const char *) new_action->commands);
                        globalMUD.action_list.remove(a);
                        delete a;
                        globalMUD.action_list.insert(new_action);
                        return;
                    }
                
                globalMUD.action_list.insert(new_action);
                status->setf ("Added action '%s' => '%s'", (const char*) new_action->pattern, (const char *) new_action->commands);
            }
        }
    }
    
    else if (!strcmp(cmd, "send")) {
        if (!currentSession)
            status->setf("No session active");
        else
            currentSession->writeMUD(s);
    }
    else if (!strcmp(cmd, "send_unbuffered")) {
        if (!currentSession)
            status->setf("No session active");
        else
            currentSession->writeUnbuffered(Sprintf("%s\n", s),strlen(s)+1);
    }
    else if (!strcmp(cmd, "reopen")) {
        if (!lastMud)
            status->setf ("There is no MUD to reconnect to");
        else
        {
            char buf[256];
            snprintf (buf, 256, "open %s", ~lastMud->name);
            mclCommand(buf);
        }
    }
    else if (!strcmp(cmd, "help"))
        status->setf("Press Alt-O, or Alt-C, or Alt-Q or RTFM");
    else if (!strcmp(cmd, "run")) {
        char fun[256];
        s = one_argument(s, fun, false);
        embed_interp->run(fun, s, NULL);
    }
    else if (!strcmp(cmd, "eval")) {
        char out[MAX_MUD_BUF];
        embed_interp->eval(s, out);
        output->printf("%s\n", out);
    } else if (!strcmp(cmd, "load"))
        embed_interp->load_file(s);
    else if (!strcmp(cmd, "setinput"))
        inputLine->set(s);
    else if (!strcmp(cmd, "magic")) {
        s = one_argument(s, cmd, false);
        if (!strcmp(cmd, "enableFunction"))
            embed_interp->enable_function(s);
        else
            status->setf("%s is not a valid callback command", cmd);
    }
    else if (!strcmp(cmd, "save")) {
        char fname[MAX_MUD_BUF];
        bool color = false;
        
        s = one_argument(s, fname, false);
        if (!strcmp(fname, "-c")) {
            color = true;
            s = one_argument(s, fname, false);
        }
        if (!fname[0])
            status->setf("Specify file to save scrollback to.");
        else
            output->saveToFile(fname, color);
    }
    
    // Chat subcommands
    else if (!strncmp(cmd, "chat.", strlen("chat."))) {
        const char *subcmd = cmd+strlen("chat.");
        if (!chatServerSocket)
            status->setf("CHAT is not active. Set chat.name option in the config file");
        else
            chatServerSocket->handleUserCommand(subcmd,s);
    }

    // Chat aliases
    else if (!strcmp(cmd, "chat"))
        mclCommand(Sprintf("%s %s", "chat.to", s));
    else if (!strcmp(cmd, "chatall"))
        mclCommand(Sprintf("%s %s", "chat.all", s));
    else {
        if (embed_interp->run_quietly(Sprintf("mclcmd_%s", cmd),s, NULL, true))
            return;
        
        status->setf ("Unknown command: %s", cmd);
    }
}

void Interpreter::setCommandCharacter(int c) {
  commandCharacter = c;
}

char Interpreter::getCommandCharacter() {
  return commandCharacter;
}
