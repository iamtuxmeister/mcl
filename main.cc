// MUD Client for Linux, main file
#include "mcl.h"
#include "cui.h"
#include "Session.h"
#include "Hotkey.h"
#include "Pipe.h"
#include "Interpreter.h"
#include "Borg.h"
#include "Curses.h"
#include "EmbeddedInterpreter.h"
#include "Plugin.h"
#include "Chat.h"
#include <signal.h>
#include <unistd.h>
#include <time.h>

#define STOP kill(getpid(),SIGSTOP) // For debugging

// Some globals

Config *config;	 // Global configuration
time_t current_time;

GlobalStats globalStats;

StatusLine *status;
MainInputLine *inputLine;
OutputWindow *output;
Screen *screen;

MUD *lastMud;
Session *currentSession;
TTY *tty;
bool mclFinished;

InterpreterPipe *interpreterPipe;
OutputPipe *outputPipe;

int session_fd = -1; // Set to somethign else if recovering from copyover

void load_history();
void save_history();

int main(int argc, char **argv) {
    if (getenv("DEBUG_MCL")) {
        STOP;
    }
    
    // Initialize vcsa screen driver, drop any setgid that we have
    //  Do this right at the start, so that a) perl can startup correctly,
    //  and b) to avoid any unforseen holes in eg. configfile loading     -N
    screen = new Screen();
    setegid(getgid());
    
    time (&current_time);
    srand(current_time);
    
    config = new Config(getenv("MCLRC"));	// Load config file
    
    // Parse command line switches: return first non-option	
    int non_option = config->parseOptions(argc, argv);

    // Load the chosen plugins
    Plugin::loadPlugins(config->getStringOption(opt_plugins));
    
    (void)new Hotkey(screen);
    
    // Create the output window which will show MUD output
    output = new OutputWindow(screen);

    // Create and insert input line
    // We want this to grow, so it should be on top of the OutputWindow
    inputLine = new MainInputLine();
    
    // Create and insert status line onto screen
    status = new StatusLine(screen);
    
    output->printVersion();
    
    interpreter.setCommandCharacter((char)config->getOption(opt_commandcharacter));
    load_history();
    
    // Initialize keyboard driver
    tty = new TTY();
    
    screen->clear();

    // Create pipes to interpreter and screen
    interpreterPipe = new InterpreterPipe;
    outputPipe = new OutputPipe;
    signal(SIGPIPE, SIG_IGN);
    
    // We want error messages from the interpreter to appear on our screen
    // (which is connected to the read end of the STDOUT_FILENO pipe)
    if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0)
        error ("dup2");
    
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    embed_interp->set("now", current_time);
    embed_interp->set("interpreterPipe", interpreterPipe->getFile(Pipe::Write));
    embed_interp->set("VERSION", versionToString(VERSION));
    embed_interp->set("commandCharacter", Sprintf("%c", config->getOption(opt_commandcharacter)));
    embed_interp->run_quietly("sys/init", "", NULL);

    if (strlen(config->getStringOption(opt_chat_name)) != 0)
        ChatServerSocket::create(config->getOption(opt_chat_baseport));

    config->compileActions();    // compile triggers, this must be done after the embedded interpreter has been initialized
    
    if (argv[non_option]) {
        lastMud = config->findMud(argv[non_option]);
        if (!lastMud) {
            if (argv[non_option+1] && atoi(argv[non_option+1])) {
                // hostname portname. Create a temporary MUD entry
                lastMud = new MUD("temp", argv[non_option], atoi(argv[non_option+1]), &globalMUD);
                config->mud_list->insert(lastMud);
                
                currentSession = new Session(*lastMud, output, session_fd);
                if (session_fd == -1)
                    currentSession->open();
            }
            status->setf ("MUD %s not in database", argv[non_option]);
        } else {
            currentSession = new Session(*lastMud, output, session_fd);
            if (session_fd == -1)
                currentSession->open();
        }
    }
    else
        status->setf ("Use %copen (or Alt-O) to connect to a MUD.", interpreter.getCommandCharacter());

    Plugin::displayLoadedPlugins();
    
    // Initialize the Borg if wanted
    Borg *borg = NULL;
    if (config->getOption(opt_borg))
        borg = new Borg();
    
    for (mclFinished = false; !mclFinished; ) {
        screen->refresh(); // Update the screen here
        
        time (&current_time);
        embed_interp->set("now", current_time);
        
        Selectable::select(0, 250000);
        
        embed_interp->run_quietly("sys/postoutput", "", NULL);
        
        // Execute the stacked commands
        interpreter.execute();
        
        if (currentSession)
            currentSession->idle();		// Some time updates if necessary

        if (chatServerSocket)
            chatServerSocket->idle();
        
        screen->idle();			// Call all idle methods of all windows
        EmbeddedInterpreter::runCallouts();
        
        // This doesn't feel right
        if (currentSession && currentSession->state == disconnected) {
            delete currentSession;
            currentSession = NULL;
            inputLine->set_default_prompt();
            screen->flash();
        }
    }

    set_title("xterm");
    embed_interp->run_quietly("sys/done", "", NULL);
    Plugin::done();
    Selectable::select(0,0); // One last time for anything that was sent by the done scripts
    interpreter.execute();
    
    int session_fd = currentSession ? dup(currentSession->getFD()) : -1;

    delete chatServerSocket;
    delete currentSession;						// Close the current session, if any
    delete screen;					// Reset screen (this kills all subwindows too)
    delete tty;						// Reset TTY state
    
    delete outputPipe;
    delete interpreterPipe;
    
    freopen("/dev/tty", "r+", stdout);
    freopen("/dev/tty", "r+", stderr);
    
    if (!mclRestart) {
        fprintf (stderr, CLEAR_SCREEN
                 "You wasted %d seconds, sent %d bytes and received %d.\n"
                 "%ld bytes of compressed data expanded to %ld bytes (%.1f%%)\n"
                 "%d characters written to the TTY (%d control characters).\n"
                 "Goodbye!\n",
                 (int)(current_time - globalStats.starting_time),
                 globalStats.bytes_written,
                 globalStats.bytes_read,
                 globalStats.comp_read,
                 globalStats.uncomp_read,
                 globalStats.uncomp_read ? (float)globalStats.comp_read / (float)globalStats.uncomp_read * 100.0 : 0.0,
                 globalStats.tty_chars, globalStats.ctrl_chars
                );
    }

    bool opt_copyover_value = config->getOption(opt_copyover);

    save_history(); // OJ: better do this before config is deleted..
    
    delete borg;
    delete config;					// Save configuration; updates stats too
    
    if (mclRestart) {
        char fd_buf[64];
        sprintf(fd_buf, "%d", session_fd);
        
        char **temp_arg = new char* [6] , **arg = temp_arg+1;
        memcpy (temp_arg, argv, sizeof(char*)); // copy just first arg
        
        if (currentSession) {
            if (opt_copyover_value) {
                *arg++ = "-@";
                *arg++ = fd_buf;
            } else {
                close(session_fd);
            }
            
            *arg++ = (char*)~lastMud->name;
            *arg++ = NULL;
        }

        *arg = NULL;
        
        signal (SIGPROF, SIG_IGN);		
        execv(argv[0], temp_arg);
        
        // Don't try this at home
        temp_arg[0] = "/root/ed/mcl/mcl";
        execv(argv[0], temp_arg);
        
        // OK, last attempt. path.
        temp_arg[0] = "mcl";
        execvp(argv[0], temp_arg);
        
        perror ("exec");
    }
    
    return 0;
}
