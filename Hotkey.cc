#include "mcl.h"
#include "cui.h"
#include "Hotkey.h"
#include "InputBox.h"
#include "ScrollbackSearch.h"
#include "Interpreter.h"
#include "Alias.h"
#include "Session.h"
#include "Chat.h"

Hotkey *hotkey;

bool mclRestart = false; // If true, restart the process when finished

bool Hotkey::keypress (int key)
{
    switch (key)
    {
    case key_alt_a:
        (void) new AliasSelection(screen, currentSession ? &currentSession->mud : &globalMUD, "Aliases");
        break;

    case key_alt_i:
        (void) new ActionSelection(screen, "Actions");
        break;

    case key_alt_m:
        (void) new MacroSelection(screen, "Macros");
        break;
        
    case key_alt_t:			// reexec
        mclRestart = true;
        mclFinished = true;
        break;
        
    case key_alt_q: 		// Quit
        mclFinished = true;
        break;
        
    case key_alt_r:
        interpreter.mclCommand("reopen");
        break;
        
    case key_alt_v:
        output->printVersion();
        break;
        
    case key_alt_s:
        if (!currentSession)
            status->setf ("No active session");
        else
            currentSession->show_nsw();
        break;
        
    case key_alt_o:
        (void) new MUDSelection(screen);
        break;
        
    case key_ctrl_t:
        if (!currentSession)
            status->setf ("No active session");
        else
            currentSession->show_timer();
        break;
        
    case key_alt_c:
        interpreter.mclCommand("close");
        break;

    case key_alt_h:
        if (!chatServerSocket)
            status->setf("CHAT is not active. Set chat.name option in the config file");
        else
            (void) new ChatWindow(screen);
        break;
        
    case key_alt_slash:
        (void) new ScrollbackSearch(false);
        break;
        
        // Enter scrollback					
    case key_page_up:
    case key_home:
    case key_pause:
        (new ScrollbackController(screen, output))->keypress(key);
        break;
        
    default:
        return false;
    }
    
    return true;
}
