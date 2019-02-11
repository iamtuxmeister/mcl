#include "mcl.h"
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "Action.h"
#include "cui.h"
#include "Alias.h"
#include "Plugin.h"
#include "Interpreter.h"


// This table contain names of all options, corresponding switch names
// and the variable that is to be set when the option is given

typedef enum {pt_bool, pt_int, pt_hex, pt_char, pt_string} ptype_t; // Parameter type

const struct
{
	char opt_char;		// e.g. -w. 0 = end of table
	char *name;			// Name used i config file, e.g. histwordsize=1
	ptype_t ptype;		// Is it a toggle or does it take a number?
	option_t option;	// Which option does it set?
	char *description;	// Description, for config file saving
	int default_value;	// Default value
	int min;			// Maximum value
    int max;			// Minimum value
} option_table[] =
{
	{	'?',		"commandcharacter",		pt_char, opt_commandcharacter,
		"The character to prefix internal mcl commands with",
		'#',	'!',	 '~'
	},

    {	'?',		"escapecharacter",		pt_char, opt_escape_character,
		"Lines starting with this character are passed as-is",
		'\\',	'!',	 '~'
	},

	{	'w',		"histwordsize",			pt_int,	opt_histwordsize,
		"Minimum number of chars in a command to save into history",
		5, 1, 999
		},

	{	'l',		"scrollback",			pt_int, opt_scrollback_lines,
		"Number of lines in scrollback",
		10000, 5000, 250000
		 },

	{	'H',		"histsize",				pt_int,	opt_histsize,
		"Number of lines saved in the command history",
		25, 5, 1000
		 },
	
	{	's',		"showprompt",			pt_bool,opt_showprompt,
		"Should the prompt be echoed on the main output screen?",
		false, false, true
		 },
		
	{	'e',		"echoinput",			pt_bool,opt_echoinput,
		"Should input be sent to MUD be echoed on the main output screen?",
		false, false, true
		},		

	{	'b',		"beep",					pt_int,opt_beep,
		"What frequency to beep with on errors? 0 disables, max 10",
		1, 0, 10
		},		
	
	{	'r',		"readonly",				pt_bool, opt_readonly,
		"If set, configuration file will not be saved on exiting",
		false, false, true
	},

    {	'r',		"nodefaults",	   		pt_bool, opt_nodefaults,
		"If set, only options that do not equal their default are saved in the config file",
		false, false, true
	},
    {	'r',		"save_history",	   		pt_bool, opt_save_history,
		"Save the input history between sessions into ~/.mcl/history",
		true, false, true
	},
	
	{	'W',		"historywindow",		pt_int,	opt_historywindow,
		"Number of lines in the pop-up history window. 0 disables",
		10,	0,	25
	},
	
	{	'B',		"mudbeep",				pt_bool,	opt_mudbeep,
		"Should beeps from the MUD be honored?",
		false, false, true
	},
	
	{	'T',		"tabsize",				pt_int,		opt_tabsize,
		"Tabstop size",
		8,	2,	16
	},

	{	'?',		"statcolor",			pt_hex,		opt_statcolor,
		"Color of the stat window (activated with Alt-S); see README for values",
		bg_green|fg_red, 0, 255
	},
	
	{	'?',		"inputcolor",			pt_hex,		opt_inputcolor,
		"Color of the input line",
		bg_cyan|fg_black, 0, 255
	},
	
	{	'?',		"statuscolor",			pt_hex,		opt_statuscolor,
		"Color of the status line",
		bg_cyan|fg_black, 0, 255
	},
	
	{	'?',		"autostatwin",			pt_bool,	opt_autostatwin,
		"Automatically bring up the stat window",
		false, false, true
	},
	
	{	'?',		"speedwalk",			pt_bool,	opt_speedwalk,
		"Speedwalk enabled? (toggle in mcl with #enable speedwalk)",
		true, false, true
	},
	
	{	'?',		"timercolor",			pt_hex,		opt_timercolor,
		"Color of timer window",
		bg_cyan|fg_black,0,255
	},
	
	{	'?',		"autotimerwin",			pt_bool,	opt_autotimerwin,
		"Automatically bring up the clock/timer window",
		true,	false,	true
	},
	
	{	'?',		"timerstate",			pt_int,		opt_timerstate,
		"Default timer state (from 0 to 6)",
		0,		0,		6
    },

    {   '?',        "borg",                 pt_bool,    opt_borg,
        "Check for a new version of mcl on startup",
        true,       false,  true
    },

    {   '?',        "borg_verbose",         pt_bool,    opt_borg_verbose,
        "If BORG is set, show some extra info to user",
        true,       false,  true
    },

    {   'D',        "interp_debug",           pt_bool,    opt_interpdebug,
        "Display debugging messages for embedded interpreter",
        true,       false,  true
    },

    {   '?',        "multiinput",           pt_bool,    opt_multiinput,
        "Input line expands to multiple lines automatically",
        true,       false,  true
    },

    {   '?',        "copyover",             pt_bool,     opt_copyover,
        "Use the hot reconnect feature when using Alt-T",
        false,       false,   true
    },

    {   '?',        "snarf_prompt", pt_bool, opt_snarf_prompt,
        "Change the prompt when a new one is recieved",
        true, false, true
    },

    {   '?',        "speedwalk_character", pt_char, opt_speedwalk_character,
        "What character starts a speedwalk",
        '^', '!', '~'
    },

    {   '?',        "expand_semicolon", pt_bool, opt_expand_semicolon,
        "Is ; expanded on the command line or just in aliases",
        false, false, true
    },

    {   'p',        "plugins",          pt_string, opt_plugins,
        "Dynamically loaded modules (seperate with ,)",
        0,0,0,
    },

    // MudMaster/ZCHAT chat specific stuff
    {
        '?',        "chat.name",       pt_string,    opt_chat_name,
        "Your name in the MudMaster chat system (see doc/Chat). If set to nothing, chat is fully disabled",
        0,0,0
    },
    {
        '?',        "chat.port",       pt_int,    opt_chat_baseport,
        "Base port to listen on for MudMaster chat. mcl will try up to this+10 ports",
        4050, 1, 65535
    },

    {
        '?',        "chat.autoaccept",       pt_bool,    opt_chat_autoaccept,
        "Automatically accept all incoming chat connections or ask for confirmation?",
        false, false, true
    },

    {
        '?',        "chat.nodisturb",       pt_bool,    opt_chat_nodisturb,
        "Automatically reject all incoming chat connections?",
        false, false, true
    },

    {
        '?',        "chat.debug",       pt_bool,    opt_chat_debug,
        "Show extra debug information about chat requests?",
        false, false, true
    },

    {
        '?',        "chat.download",       pt_string,    opt_chat_download,
        "Directory where files transferred via chat are placed (default: ~/.mcl/download/)",
        0,0,0
    },

    {
        '?',        "chat.email",       pt_string,    opt_chat_email,
        "E-mail address displayed to others",
        0,0,0
    },

    {
        '?',        "chat.interfaces",       pt_string,    opt_chat_interfaces,
        "What network interfaces should mcl look at to determine what IP address to use? Delimiter with space (default: ppp0 eth0 lo)",
        0,0,0
    },

    {
        '?',        "chat.icon",       pt_string,    opt_chat_icon,
        "Icon file sent to others",
        0,0,0
    },

    {
        '?',        "chat.protocol",       pt_bool,    opt_chat_protocol,
        "0 to use standard Chat, 1 to use zChat",
        1,0,1
    },

    {
        '?',        "chat.paranoia",       pt_bool,    opt_chat_paranoia,
        "Prefix all messages with their source (e.g. [Drylock@127.0.0.1] Drylock chats 'hi'.",
        0,0,1
    },

    {
        '?',        "chat.chatcolor",       pt_hex,    opt_chat_chatcolor,
        "Color of messages from others (e.g. CHAT: Drylock chats 'hullo'",
        bg_black|fg_yellow|fg_bold, 0, 255
    },

    {
        '?',        "chat.syscolor",       pt_hex,    opt_chat_syscolor,
        "Color of system messages (e.g. CHAT: Connection to 127.0.0.1 failed)",
        bg_black|fg_white, 0, 255
    },

    {   0, 0, pt_bool, max_option, 0, 0, 0, 0 }
};

// Old obsolete options
static const char *obsolete_options[] = {
    "ttyok", "embedded_interpreter",
    NULL
};
    

// Load the config file. if fname = NULL, default to ~/.mclrc
void    Config::Load (const char *fname)
{
    FILE   *fp;
    char    buf[INPUT_SIZE], mudname[INPUT_SIZE], hostname[INPUT_SIZE];
    char    commands[INPUT_SIZE];
    int     port, count;
    struct stat stat_buf;
    MUD * last_mud = &globalMUD;
    const char *home = getenv("HOME");
    
    if (!home)
        error ("The HOME environment variable is not set. This is bad.");
    
    if (fname) {
        fp = fopen (fname, "r");
        filename = fname;
    }
    else
    {
        snprintf (buf, 256, "%s/.mcl/mclrc", home);
        if (!(fp = fopen(buf, "r")) && errno == ENOENT) {
            snprintf (buf, 256, "%s/.mclrc", home);
            fp = fopen (buf, "r");
        }
        
        filename = buf;
    }
    
    if (!fp) {
        if (errno == ENOENT) {
            fprintf (stderr, "Could not open config file: %s (%m)\n", fname ? fname : buf);
            fprintf (stderr, "I will create the directory %s/.mcl for you and a sample configuration file inside it.\n", home);
            fprintf (stderr, "When you leave mcl, the file will contain all the default settings.\n");
            sprintf(buf, "%s/.mcl", home);
            if (mkdir(buf ,0700) < 0)
                error("I failed creating the directory. I give up now (%m)!\n");
            sprintf(buf, "%s/.mcl/mclrc", home);
            if (!(fp = fopen(buf, "w")))
                error("I failed (%m). I give up now.\n");
            fprintf(fp, "# Automatically generated. It should get overwritten with a lot of setting next time you exit mcl.\n");
            fchmod(fileno(fp), 0600);
            fclose(fp);
            fprintf(stderr, "OK. The file was created. Press enter to confirm this happy news.\n");
            fgets(buf, 1, stdin);
            
            Load(fname);
            return;
        }
    }
    
    
    fstat (fileno (fp), &stat_buf);
    
    if (stat_buf.st_mode & 077)
        error ("Your configuration file is publically accessible.\n"
               "Please use chmod 600 %s before proceeding.\n", ~filename);
    
    mud_list = new MUDList;
    save_time = stat_buf.st_mtime; // save the time
    
    while (fgets (buf, INPUT_SIZE, fp))
    {
        int     match, len;
        char    name[INPUT_SIZE], keyword[INPUT_SIZE];
        char *nl , *value;
        const char *rest;

        if (buf[0] == '#')		// comment
            continue;
        
        if ((nl = strchr(buf, '\n')))
            *nl = NUL;
        
        rest = one_argument(buf, keyword, true);
        len = 0;
        
        if (!strcmp(keyword, "macro") ||
            !strcmp(keyword, "alias") ||
            !strcmp(keyword, "action") ||
            !strcmp(keyword, "subst")) {
            parseMUDLine(keyword, rest, last_mud);
        }
        // An option? (weirdness so we can have an empty line after =)
        // Note: the sscanf may succeed matching ^= but not match =. In that case len will
        // remain 0
        else if (1 == sscanf (buf, "%[^=] =%n", name, &len) && len && buf[len-1] == '=') {
            int i, option_value;
            value = buf+len;
            while (isspace(*value))
                value++;

            for (i = 0; option_table[i].name; i++)
                if (!strcasecmp (option_table[i].name, name)) {
                    
                    if (option_table[i].ptype == pt_string)
                        setStringOption(option_table[i].option, value);
                    else {
                        if (option_table[i].ptype == pt_char)
                            option_value = value[0];
                        else
                            if ((1 != sscanf(value, "0x%x", &option_value))
                                && (1 != sscanf(value, "%d", &option_value)))
                                error ("Malformed number: %s\n",value);

                        setOption (option_table[i].option, option_value);
                    }
                    break;
                }
            
            if (!option_table[i].name) { // hmm, not found
                for (i = 0; obsolete_options[i]; i++)
                    if (!strcasecmp(name, obsolete_options[i]))
                        goto NEXT_LINE;
                
                error ("Unrecognized option in configuration file: %s", name);
            }
        }
        
        else if (!strcmp(keyword, "mud")) {
            if (1 != sscanf(rest, "%s {", name))
                error ("Invalid MUD line: Must be MUD <mudname> {");
            readMUD(fp, name);
        }
        else					// mud data. or nothing.
        {
            match = sscanf (buf, " %512s %512s %d%n", mudname, hostname, &port, &count);
            
            if (match <= 0)
                continue;
            else if (match != 3)
                error ("Invalid line: You did not specify a valid configuration option, or you did\n"
                       "not specify a full MUD definition (MUD name, hostname and port):\n%s\n", buf);
            
            if (1 != sscanf (buf + count, " %[^\n]", commands))
                commands[0] = NUL;
            
            mud_list->insert ((last_mud = new MUD (mudname, hostname, port, &globalMUD, commands)));
        }
        
    NEXT_LINE:
        ;
    }
    
    fclose (fp);
}

// Parse this MUD command which can appear within a MUD { } section or in the old-style format
void Config::parseMUDLine(const char *keyword, const char *buf, MUD *mud) {
    char name[INPUT_SIZE], value[INPUT_SIZE];
    
    if (!strcmp(keyword, "macro")) {
        if (2 != sscanf (buf, "%s %[^\n]", name, value))
            error ("Invalid Macro format, must be Macro <key> <command>:\n%s\n", buf);
        
        int key = key_lookup(name);
        
        if (key < 0)
            error ("Unknown key in macro definition: %s", buf);
        mud->macro_list.insert(new Macro(key,value));
    }
    else if (!strcmp(keyword, "alias")) {
        if (2 != sscanf (buf, "%s %[^\n]", name, value)) // alias?
            error ("Invalid Alias format, must be Alias <name> <commands>:\n%s\n", buf);
        mud->alias_list.insert(new Alias(name,value));
    }
    else if (!strcmp(keyword, "action")) {
        String res;
        Action *ac = Action::parse(buf, res, Action::Trigger);
        
        if (!ac)
            error (res);
        
        mud->action_list.insert(ac);
    }
    else if (!strcmp(keyword, "subst")) {
        String res;
        Action *ac = Action::parse(buf, res, Action::Replacement);
        
        if (!ac)
            error (res);
        
        mud->action_list.insert(ac);
    } else
        error ("Uh, parsedMUDLine() called with invalid keyword %s?!", keyword);
}

// Read in the data about a MUD
void Config::readMUD(FILE *fp, const char *mudname) {
    MUD *mud = new MUD(mudname, "", 0, &globalMUD);
    char buf[INPUT_SIZE], name[INPUT_SIZE];
    char keyword[INPUT_SIZE];
    char *nl;
    const char *s;
    bool done = false;
    int port;
    
    while ((fgets(buf, sizeof(buf), fp))) {
        if ((nl = strchr(buf, '\n')))
            *nl = NUL;
        
        for (s = buf; *s && isspace(*s); s++)
            ;
        
        if (*s == '}') {
            done = true;
            break;
        }
        
        if (*s == '#' || !*s)
            continue;
        
        s = one_argument(s, keyword, true);
        
        if (!strcmp(keyword, "macro") ||
            !strcmp(keyword, "alias") ||
            !strcmp(keyword, "action") ||
            !strcmp(keyword, "subst")) {
            parseMUDLine(keyword, s, mud);
        } else if (!strcmp(keyword, "host")) {
            if (2 != sscanf(s, "%s %d", name, &port))
                error ("Invalid Host line in MUD %s definition: %s", mudname, s);
            mud->setHost(name, port);
        } else if (!strcmp(keyword, "inherit")) {
            MUD *parent = mud_list->find(s);
            if (!parent)
                error ("Parent MUD %s not found in MUD %s definition", s, mudname);
            mud->inherits = parent;
        } else if (!strcmp(keyword, "commands")) {
            mud->commands = s;
        } else
            error ("Invalid keyword %s within mud %s definition:\n%s\n", keyword, mudname, buf);
    }
    
    if (!done)
        error ("MUD entry %s was not propertly terminated with a }\n", mudname);
    mud_list->insert(mud);
}


MUD * Config::findMud (const char *name) {
	return mud_list->find(name);
}

static const char *type_to_string(ptype_t t) {
    switch (t) {
    case pt_int:  return "<num>";
    case pt_hex:  return "<hex color>";
    case pt_char: return "<char>";
    case pt_bool: return "<0|1>";
    default:
        return "<?>";
    }
}

// Parse command line options
int Config::parseOptions (int argc, char **argv) {
    int i,c;
    char option_string[max_option*2+1];
    char *pc;
    
    // Build an option string based on the option_table
    for (pc = option_string, i = 0; option_table[i].opt_char; i++) {
        if (option_table[i].opt_char != '?')  {
            *pc++ = option_table[i].opt_char;
            if (option_table[i].ptype == pt_int || option_table[i].ptype == pt_hex || option_table[i].ptype == pt_string)
                *pc++ = ':';
            else if (option_table[i].ptype == pt_bool) {
                *pc++ = ':';
                *pc++ = ':';
            }
        }
    }
    
    *pc++ = '@'; *pc++ = ':';
    *pc++ = 'x'; *pc++ = ':';
    *pc = NUL;
    
    while( (c = getopt(argc,argv, option_string)) != EOF)  {
        // Help requested or error ocurred, show options
        if (c == '?' || c == 'h')  {
            fprintf (stderr,
                     "\nmcl - my MUD client version %s\n"
                     "Usage: mcl [options] [mud alias]\n\n",
                     versionToString(VERSION)
                    );
            
            for (i = 0; option_table[i].opt_char; i++)
                if (option_table[i].opt_char != '?') {
                    fprintf(stderr, "-%c%-10s\t%s",
                            option_table[i].opt_char,
                            type_to_string(option_table[i].ptype),
                            option_table[i].description);
                    
                    if (option_table[i].ptype == pt_int) {
                        fprintf(stderr, " (%d-%d)", option_table[i].min, option_table[i].max);
                    }
                    fprintf(stderr, "\n");
                }
            
            exit (EXIT_FAILURE);
        } else if (c == '@') { // Recovering from alt-T
            extern int session_fd;
            session_fd = atoi(optarg);
        } else if (c == 'x') { // execute command
            interpreter.add(optarg);
        } else
            for (i = 0; option_table[i].opt_char; i++)	
                if (option_table[i].opt_char == c)
                {
                    if (option_table[i].ptype == pt_int) // Integer option
                        setOption(option_table[i].option, atoi(optarg));
                    else if (option_table[i].ptype == pt_char) // char
                        setOption(option_table[i].option, optarg[0]);
                    else if (option_table[i].ptype == pt_string)
                        setStringOption(option_table[i].option, optarg);
                    else // Toggle (hmm, we don't even have any boolean options now?
                    {
                        if (optarg)
                            setOption(option_table[i].option, atoi(optarg) ? true : false);
                        else
                            setOption(option_table[i].option, true);
                    }
                    
                    break;					
                }
    }
    
    // Validate options
    
    bool failed = false;
    
    for (i = 0; option_table[i].name; i++)
        if (getOption(option_table[i].option) > option_table[i].max) {
            fprintf (stderr, "Option '%s' has value %d (max is %d)\n",
                     option_table[i].name,
                     getOption(option_table[i].option), option_table[i].max);
            
            failed = true;
        } else if (getOption(option_table[i].option) < option_table[i].min) {
            fprintf (stderr, "Option '%s' has value %d (min is %d)\n",
                     option_table[i].name,
                     getOption(option_table[i].option), option_table[i].min);
            
            failed = true;
        }
    
    if (failed)
        error ("One or more options had values outside allowed range.");
    
    return optind;
}

// Load config. If fname == NULL, use ~/.mclrc
Config::Config (const char *fname) {
	// Set all config options to default values
	for (int i = 0; option_table[i].name; i++)
        setOption (option_table[i].option, option_table[i].default_value);

    // Move this later to a table once there is more than one string option
    setStringOption(opt_plugins, "perl");
    setStringOption(opt_chat_download, Sprintf("%s/.mcl/downloads/", getenv("HOME")));
    setStringOption(opt_chat_interfaces, "ppp0 eth0 lo");

    Load(fname);
    String icon_file = getStringOption(opt_chat_icon);
    if (icon_file.len() && access(icon_file, R_OK) != 0)
        error ("Icon file %s was specified, but does not exist", ~icon_file);
}

// Save the configuration
void Config::Save(const char *fname) {
    struct stat stat_buf;
    String name;
    
    if (fname == NULL)
        name = filename;
    else
        name = fname;
    
    // Check that there have been no modifications
    if (stat(name, &stat_buf) == 0 && stat_buf.st_mtime != save_time)
        fprintf(stderr, "Configuration file was NOT saved. It has been changed since it was last loaded!\n");
    else  {
        FILE *fp = fopen (name, "w");
        if (!fp)
            return;
        
        fprintf (fp, "# mcl configuration file, generated by mcl v%s on %s\n",
                 versionToString(VERSION), ctime(&current_time));
        
        for (int i = 0; option_table[i].name; i++) {
            if (option_table[i].ptype == pt_string)
                fprintf(fp, "# %s\n%s=%s\n\n", option_table[i].description, option_table[i].name, ~getStringOption(option_table[i].option));
            else {
                if (!config->getOption(opt_nodefaults) || ( getOption(option_table[i].option) != option_table[i].default_value))
                    fprintf (fp, option_table[i].ptype == pt_hex ? "# %s\n%s=0x%02x\n\n" : option_table[i].ptype == pt_char ? "# %s\n%s=%c\n\n" : "# %s\n%s=%d\n\n",
                             option_table[i].description,
                             option_table[i].name,
                             getOption(option_table[i].option));
            }
        }
        
        globalMUD.write(fp, true);
        
        // Save all MUDs but those marked as temporary
        FOREACH(MUD*, mud, (*mud_list))
            if (mud->name != "temp") {
                fprintf(fp, "\n");
                mud->write(fp, false);
            }
        
        fclose (fp);
    }
}

Config::~Config() {
    if (!getOption(opt_readonly))
        Save();
}

void Config::compileActions() {
    for (MUD* mud= mud_list->rewind(); mud; mud = mud_list->next())
        if (strcmp(mud->name, "temp")) {
            // Actions
            FOREACH(Action*, action, mud->action_list)
                action->compile();
        }
    
    FOREACH(Action*, action2, globalMUD.action_list)
        action2->compile();
    
}

