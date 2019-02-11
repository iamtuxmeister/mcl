enum val_type {val_quit, val_ok}; // Cancel or OK?


class Group : public Window
{
	public:
	Group(Window *_parent, int _w, int _h, int _x, int _y);
	
	private:
	virtual void redraw();
	virtual void remove (Window *window);
	virtual void insert (Window *window);
	virtual bool validate(val_type val) = 0;
	virtual void finish() = 0;
	virtual bool keypress(int key);
};

class MUDEdit : public Group
{
	public:
	MUDEdit(Window *_parent, MUD *_mud);
	
	private:
	MUD *mud;
	InputLine *mud_name, *mud_hostname, *mud_port, *mud_commands;
	
	virtual bool validate(val_type val);
	virtual void finish();
	virtual void redraw();
};
