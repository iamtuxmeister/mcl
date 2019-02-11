class ScrollbackController;


class OutputWindow : public Window
{
public:
    OutputWindow(Window *_parent);
    ~OutputWindow();
    
    void printVersion();
    NAME("OutputWindow");

    void search (const char *s, bool forward);
    virtual void move(int, int);
    void saveToFile(const char *fname, bool color); // Save scrollback to file
    
private:
    
    typedef enum { 
        move_home, move_page_up, move_line_up, move_stay, 
        move_line_down, move_page_down
    } move_t;
    
    virtual bool scroll();
    
    virtual void draw_on_parent();
    
    void freeze()   { fFrozen = true; }	// Freeze viewpoint
    void unfreeze() { fFrozen = false; viewpoint = canvas; } // Unfreeze
    
    bool moveViewpoint(move_t dir);	// Move the viewpoint. true if reached end of buffer
    
    attrib 	*scrollback; // Beginning of the buffer
    attrib  *viewpoint;	 // What are we viewing?

    int top_line;        // What is the number of the top line in the scrollback?
    struct
    {
        int line;        // What line # to highlight?
        int x;           // What x offset to start at ?
        int len;         // How much to highlight?
    } highlight;
    
    bool	fFrozen;	// Should we move viewpoint or not?
    friend class ScrollbackController;
};

// This is an invisible object that controls the scrollback process
class ScrollbackController : public Window
{
public:
    ScrollbackController(Window *_parent, OutputWindow *_output) 
        : Window(_parent,0,0,None), output(_output) {output->freeze(); }
    
    virtual bool keypress(int key);

    NAME("ScrollbackController");
    
private:
    OutputWindow *output;
};

extern OutputWindow *output;

