// The command interpreter holds commands to be executed

// Expand what?
#define EXPAND_NONE      0x00
#define EXPAND_VARIABLES 0x01
#define EXPAND_ALIASES   0x02
#define EXPAND_SEMICOLON 0x04
#define EXPAND_SPEEDWALK 0x08
#define EXPAND_ALL       0xffff

// Default flags for entry from the input line
#define EXPAND_INPUT     (EXPAND_ALIASES|EXPAND_SPEEDWALK)

class Interpreter {
public:
    void add(const char *s, int flags = EXPAND_ALL, bool back = true);
    void execute();
    void mclCommand (const char *command);
    void setCommandCharacter (int c);
    char getCommandCharacter();
    void expandAliases(const char*, int flags);
    const char *expandVariables(const char *);
    void expandSemicolon(const char*, int);
    void expandSpeedwalk(const char*, int );
    
private:
    List<String*> commands;
    char commandCharacter;
};

extern Interpreter interpreter;

extern bool macros_disabled, aliases_disabled, actions_disabled;
