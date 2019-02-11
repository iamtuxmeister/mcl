// This is a small derivative of Window that is numbered and
// can be written to and created by the user and can also dump output
// coming into it to the log file

class MessageWindow: public ScrollableWindow, public Numbered {
public:
    MessageWindow(Window *_parent, const char *_alias, const char *_logfile, int _w, int _h, int _x, int _y, Style _style, int _timeout,
                  bool _popOnInput, int _color);
    
    static List<MessageWindow*> list;
    static MessageWindow* find(const char *_name);
    
    void addInput(const char *s)
    {
        if (popOnInput)
        {
            show(true);
            popUp();
        }
        
        printf("%s\n", s);
        if (logfile.len())
        {
            FILE *fp = fopen(logfile, "a");
            if (fp)
            {
                fprintf(fp, "%s\n", s);
                fclose(fp);
            }
        }
    }

    virtual void idle()
    {
        if (visible && last_input+timeout < current_time)
            show(false);
    }

    virtual void popUp()
    {
        last_input = current_time;
        Window::popUp();
    }

    ~MessageWindow()
    {
        list.remove(this);
    }
    
private:
    String alias; // What is the window called?
    String logfile; // Name of logfile to log to (if not empty)
    bool popOnInput; // Should the window show itself when new input arrives?
    int timeout; // How long before hiding the window?
    time_t last_input; // When did we last get some input from this Window?
    int default_color; // set to that before clear etc.
};
