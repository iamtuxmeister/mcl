
class Buffer;
class Screen  : public Window {
	int fd;				// For /dev/vcsa?
	
	public:
	Screen();			// Dimensions are figured out automatically
	virtual ~Screen();
	virtual bool refresh();		// Copy to the physical screen
	void set_cursor (int _x, int _y);
    virtual bool keypress(int key);
	
    void flash();		// Flash-beep
    void setScrollingRegion(int,int,int,int);
    bool isVirtual() { return usingVirtual; }

    const char* getColorCode (int color, bool setBackground) const;

private:
    // tty version
    attrib *last_screen;
    int scr_x, scr_y, scr_w, scr_h;
    bool usingVirtual;
    Buffer *out;

    bool refreshVirtual();
    bool refreshTTY();
    void setTTYColor(int,bool);
    inline void printCharacter(int c, bool& acs_enabled);

};

extern Screen *screen;
