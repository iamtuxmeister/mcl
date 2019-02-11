#include "mcl.h"
#include "cui.h"
#include "MessageWindow.h"
#include "ProxyWindow.h"
#include "Curses.h"
#include <stdarg.h>

Window* Numbered::list[10];

Window::Window(Window *_parent, int _width, int _height,  Style _style, int _x, int _y)
    : parent(_parent), width(_width), height(_height),
    next(NULL), prev(NULL), child_first(NULL), child_last(NULL), visible(true),
    color(fg_white|bg_black),focused(NULL), parent_x(_x), parent_y(_y), cursor_x(0),
    cursor_y(0), dirty(true),  style(_style)
{
	if (width == wh_half)
		width = parent->width/2;
	else if (width == wh_full)
            width = parent->width-1;
	
	if (height == wh_half)
		height = parent->height/2;
	else if (height == wh_full)
		height = parent->height;

	if (parent_x == xy_center)
		parent_x = parent->width / 2 - width / 2;
	else if (parent_x < 0)
		parent_x = parent->width + parent_x;
		
	if (parent_y == xy_center)
		parent_y = parent->height / 2 - height / 2;
	else if (parent_y < 0)
        parent_y = parent->height + parent_y;

    // Bordered?
    if (style & Bordered) {
        Window *w = new Border(parent, width, height, parent_x, parent_y);
        width -= 2;
        height -= 2;
        parent_x = parent_y = 1 ;
        parent = w;
    }

	assert(width >= 0);
    assert(height >= 0);

    clearLine = new attrib[width];
    for (attrib *a = clearLine; a < clearLine+width; a++)
        *a =  ((fg_white|bg_black) << 8) + ' ';
    
	canvas = new attrib[width*height];
	clear();

	if (parent)
		parent->insert(this);
}

bool Window::is_focused()
{
	if (!parent || !parent->focused || parent->focused == this)
		return true;
	else
		return false;
}

void Window::insert (Window *window)
{
    if (find(window))
        abort();
        
	if (child_last)
	{
		child_last->next = window;
		window->prev = child_last;
	}
	else
		child_first = window;

    child_last = window;
    check();
}

void Window::check()
{
    Window *w;
    int count;

    for (count = 0, w = child_first; w; w = w->next)
        if (++count > 100)
            abort();

    for (count = 0, w = child_last; w; w = w->prev)
        if (++count > 100)
            abort();
}

void Window::show(bool vis) {
    visible = vis;
    dirty = true;
    if (parent)
        parent->visibilityNotify(this, vis);
}

void Window::remove (Window *window)
{
    if (!find(window))
        abort();
    
	if (window->prev)
		window->prev->next = window->next;
	
	if (window->next)
		window->next->prev = window->prev;
	
	if (window == child_first)
		child_first = window->next;
	
	if (window == child_last)
        child_last = window->prev;

    window->next = window->prev = NULL;
		
    dirty = true;
    if (focused)
        focused->dirty = true;

    if (window == focused)
        focused = NULL;
    
    check();
}

Window::~Window()
{
	Window *w, *w_next;
	// Kill all children
	
	for (w = child_first; w; w = w_next)
	{
		w_next = w->next;
		delete w;
	}

	// Remove me
	if (parent)
		parent->remove(this);
		
	delete[] canvas;
}

Window* Window::findByName(const char *name, Window *after)
{
    Window *w;
    for (w = after ? after->next : child_first ; w; w = w->next)
        if (!strcmp(name, w->getName()))
            return w;
    
    return NULL;
}

bool Window::scroll()
{
	return false;
}

#define MAX_PRINTF_BUF 16384

void Window::print(const char *s)
{
	const unsigned char *in;
	attrib *out;
	
    dirty = true;

	// Position out pointer at the beginning of canvas to write to	
	out = canvas + cursor_y * width + cursor_x;
	
	for (in = (unsigned char *)s; *in; in++)
	{
		if (*in == SET_COLOR) 
        {
            switch (in[1]) {
            case 0: // ??? black on black?
                break;
                
            case COL_SAVE:
                saved_color = color; in++;
                break;

            case COL_RESTORE:
                color = saved_color; in++;
                break;

            default:
                color = *++in;
            }
		} 
		else if (*in == SOFT_CR) // change to a new line if not there already
		{
			if (cursor_x)
			{
				cursor_x = 0; 
				cursor_y++;
				out = canvas + cursor_y * width + cursor_x;
			}
		}
		else if (*in == '\t') // tabstop
		{
			// Hop to the next tabstop. If we go over the limit, set cursor
			// to be = width, then wordwrap will kick in below
			cursor_x = min(width, ((cursor_x / config->getOption(opt_tabsize)) + 1) 
				* config->getOption(opt_tabsize));
		}
		else if (*in == '\n') // Change line
		{
			cursor_x = 0; 
			cursor_y++;
			out = canvas + cursor_y * width + cursor_x;
		}
		else
		{

			// Need to scroll? If we have to, and fail, don't print more
			// If scroll succeeds, it is assumed to have adjusted cursor_x/y
			// Note that we do not scroll until we actually WRITE something
			// at the bottom line!
			while (cursor_y >= height)
				if (!scroll())
					return;

			// Recalculate where our output is now that there has been a scroll	
			out = canvas + cursor_y * width + cursor_x;
			
			*out++ = (color << 8) + *in;
			cursor_x++;
		}
		
		// Wordwrap required?
		if (cursor_x == width)
		{
			cursor_y++;
			cursor_x = 0;
		}
		
	}
}

// Formatted print
void Window::printf(const char *fmt, ...)
{
	char buf[MAX_PRINTF_BUF];
	va_list va;
	
	va_start (va,fmt);
	vsnprintf (buf, MAX_PRINTF_BUF, fmt, va);
	va_end (va);
	
	print(buf);
}

// Formatted, centered print
void Window::cprintf(const char *fmt, ...)
{
	char buf[MAX_PRINTF_BUF];
	va_list va;
	
	va_start (va,fmt);
	vsnprintf (buf, MAX_PRINTF_BUF, fmt, va);
    va_end (va);

    gotoxy(0, cursor_y);
    if ((int)strlen(buf) > width)
        buf[width] = NUL;

    printf("%*s%s", (width-strlen(buf))/2, "", buf);
}

// Copy data from attrib to x,y. Data is w * h
void Window::copy (attrib *source, int w, int h, int _x, int _y)
{
	// Don't bother if it is clearly outside of our rectangle
    if (_y >= width || _x >= width
        || (_x+w <= 0) || (_y+h) <= 0) // far out
		return;
		
	// Direct copy?
	if (width == w && _x == 0)
    {
        // adjust if _y < 0
        if (_y < 0) {
            source += (-1 * w*_y);
            h += _y;
            _y = 0;
        }
		int size = min(w*h, (height-_y) * width);
		
		// This will not copy more than there is space for in this window
		if (size > 0)
			memcpy (canvas + _y*width, source, size * sizeof(attrib));
	}
	else
	{
		// For each row up to max of our height or source height
		for (int y2 = max(0,_y); y2 < height && y2 < (_y+h) ; y2++)
			memcpy (
				canvas + (y2 * width) + _x,				// Copy to (_x, y2)
				source + (y2 - _y) * w,					// Copy from (0, y2 - _y)
				sizeof(attrib) * min(w, width - _x));	// Copy w units, or whatever there's space for
	}
}

// This simple window cannot redraw itself
// If the canvas somehow is lost, printed stuff is lost
void Window::redraw()
{
	dirty = false;
}

bool Window::refresh()
{
	bool refreshed = false;

	// Don't do anything if we are hidden	
	if (!visible)
	{
		if (dirty)
		{
			dirty = false;
			return true;
		}
		else
			return false;
	}
	
	// Do we need to redraw?
	if (dirty)
	{
		redraw();
		refreshed = true;
	}
	
	// Refresh children
	// Note that the first in list will be the one behind
	for (Window *w = child_first; w ; w = w->next)
		refreshed = w->refresh() || refreshed;

	draw_on_parent();		 // Copy our canvas to parent

	return refreshed;		
}

// Fill this window with 'color'
void Window::clear()
{
	attrib *p;
	attrib *end = canvas + width * height;
	attrib blank = (color << 8) + ' ';
	
	for (p = canvas; p < end; p++)
		*p = blank;
	
	dirty = true;
}

// Take care of this keypress
bool Window::keypress(int key)
{
	Window *w_prev;
	
	// By default, do not do anything with the key, but just ask the children
	// If they can do anything with it
	// Keep pointer to prev window in case current gets deleted
	// This needs to be rewritten using some List/Iterator!
	for (Window *w = child_last; w; w = w_prev)
	{
		w_prev = w->prev;
		if (w->keypress(key))
			return true;
	}
	
	return false;
}

// Set hardware cursor coordinates
// Asks the parent, translating the coordinates accordingly
void Window::set_cursor(int _x, int _y)
{
	if (parent)
		parent->set_cursor(_x+parent_x,_y+parent_y);
}


void Window::idle()
{
    Window *w_next;
    for (Window *w = child_first; w; w = w_next)
    {
        w_next = w->next;
        w->idle();
    }
}

void Window::resize (int w, int h) {
    width = w; height = h;
    delete[] canvas;
    canvas = new attrib[width*height];
    clear();
    if (parent) {
        parent->force_update();
        parent->resizeNotify(this, w,h);
    }
    dirty = true;
}

void Window::move (int x, int y) {
    parent_x = x; parent_y = y;
    force_update();
    parent->force_update();
}
        


#define BRD_TOP 		(1 << 0)
#define BRD_BOTTOM 		(1 << 1)
#define BRD_LEFT 		(1 << 2)
#define BRD_RIGHT 		(1 << 3)


// Draw a box from (x1,y2) to (x2,y2) using border style _borders
void    Window::box (int x1, int y1, int x2, int y2, int _borders)
{
	int     box_width = x2 - x1;

	char   *s = new char[box_width + 1];

	// Top/bottom   
	memset (s, special_chars[bc_horizontal], box_width);
	s[box_width] = NUL;

	if (_borders & BRD_TOP)
	{
		gotoxy (x1 + 1, y1);
		print (s);
	}

	if (_borders & BRD_BOTTOM)
	{
		gotoxy (x1, y2);
		print (s);
	}

	// Left/Right
	for (int _y = y1 + 1; _y < y2; _y++)
	{
		if (_borders & BRD_LEFT)
		{
			gotoxy (x1, _y);
			printf ("%c", special_chars[bc_vertical]);
		}

		if (_borders & BRD_RIGHT)
		{
			gotoxy (x2, _y);
			printf ("%c", special_chars[bc_vertical]);
		}
	}

	// Corners
	if (_borders & BRD_TOP)
	{

		if (_borders & BRD_LEFT)
		{
			gotoxy (x1, y1);
			printf ("%c", special_chars[bc_upper_left]);
		}

		if (_borders & BRD_RIGHT)
		{
			gotoxy (x2, y1);
			printf ("%c", special_chars[bc_upper_right]);
		}
	}

	if (_borders & BRD_BOTTOM)
	{

		if (_borders & BRD_LEFT)
		{
			gotoxy (x1, y2);
			printf ("%c", special_chars[bc_lower_left]);
		}

		if (_borders & BRD_RIGHT)
		{
			gotoxy (x2, y2);
			printf ("%c", special_chars[bc_lower_right]);
		}
	}

	delete[]s;
}

void Window::clear_line(int _y)
{
    // PROBLEM: if width changes!
    if (_y > (height-1))
        abort();
    memcpy(canvas+width*_y, clearLine, width * sizeof(attrib));
}

void Window::draw_on_parent()
{
	if (parent)
		parent->copy(canvas, width, height, parent_x, parent_y);
}

void Window::popUp()
{
    Window *_parent = parent;
    _parent->remove(this);
    _parent->insert(this);
}

void Window::die() {
    Window *p = parent;
    delete this; 
    if (p)
        p->deathNotify(this);
}

Window* Window::find(Window* w)
{
    for (Window *u = child_first; u; u = u->next)
        if (u == w)
            return w;

    return NULL;
}

// Redraw a border window
void Border::redraw() {
	set_color(fg_white|bg_blue);
	
    box(0,0,width-1,height-1, BRD_TOP|BRD_LEFT|BRD_RIGHT|BRD_BOTTOM);
	
	// paint top/bottom messages
	
	if (top_message.len()) {
		gotoxy(1,0);
		printf("%-*.*s", min(strlen(top_message),width-2), min(strlen(top_message),width-2), ~top_message);
	}

	if (bottom_message.len()) {
		gotoxy(1,height-1);
		printf("%-*.*s", min(strlen(bottom_message),width-2), min(strlen(bottom_message), width-2), ~bottom_message);
	}
	
	dirty = false;
}

void Border::set_top_message(const char *s) {
    top_message = s;
	dirty = true;
}

void Border::set_bottom_message (const char *s)  {
    bottom_message = s;
	dirty = true;
}

bool ScrollableWindow::scroll()
{
	memmove (canvas, canvas+width, sizeof(*canvas) * width*(height-1));
	
	clear_line(height-1);
    cursor_y--;
	return true;
}


List<MessageWindow*> MessageWindow::list;

MessageWindow* MessageWindow::find (const char *s)
{
    for (MessageWindow *m = list.rewind(); m; m = list.next())
        if (m->alias == s)
            return m;

    return NULL;
}

MessageWindow::MessageWindow (Window *_parent, const char *_alias, const char *_logfile, int _w, int _h, int _x, int _y, Style _style, int _timeout, bool _popOnInput, int _color)
:   ScrollableWindow(_parent, _w, _h, _style, _x, _y), Numbered(this),  alias(_alias), logfile(_logfile), popOnInput(_popOnInput), timeout(_timeout), default_color(_color)
{
    char buf[256];
    if (number == -1)
        buf[0] = NUL;
    else
        sprintf(buf, "[%d] ", number);

    sprintf(buf+strlen(buf), "%s ", _alias);
    if (*_logfile)
        sprintf(buf+strlen(buf), " => %s", _logfile);
    
//    set_top_message(buf);
    
    last_input = current_time;
    list.insert(this);
    set_color(default_color);
    clear();
}

void messageBox(int wait, const char *fmt, ...)
{
    va_list va;
    char buf[MAX_MUD_BUF];
    
    va_start(va, fmt);
    vsnprintf(buf, sizeof(buf), fmt, va);
    va_end(va);
    
    (void)new MessageBox(screen, buf, wait);
}

MessageBox::MessageBox(Window *_parent, const char *_message, int _wait)
:    Window(_parent, max(_parent->width/2, 3 + longestLine(_message)),
                    countChar(_message, '\n') + 3 + (_wait ? 0 : 1), Bordered, xy_center, xy_center), message(_message),
    creation_time(current_time), wait(_wait)
{
    char line[MAX_MUD_BUF];
    char *s;
    
    set_color(bg_blue|fg_bold|fg_white);
    clear();
    
    strcpy(line, _message);
    s = strtok(line, "\n");
    
    while (s)
    {
        if (*s == *CENTER)
        {
            cprintf("%s", s+1); // need to center
            print("\n");
        }
        else
            printf("%s\n", s);
        
        s = strtok(NULL, "\n");
    }
    
    if (!wait)
    {
        set_color(bg_blue|fg_bold|fg_green);
        print ("\n");
        cprintf ("Press ENTER to continue");
    }
}

void Window::set_top_message(const char *s) {
    if (parent)
        parent->set_top_message(s);
}

void Window::set_bottom_message(const char *s) {
    if (parent)
        parent->set_bottom_message(s);
}

void Window::resizeNotify(Window *who, int w, int h) {
    if (parent)
        parent->resizeNotify(who, w, h);
}

void Window::visibilityNotify(Window *who, bool visible) {
    if (parent)
        parent->visibilityNotify(who, visible);
}

void Window::moveNotify(Window *who, int w, int h) {
    if (parent)
        parent->moveNotify(who, w, h);
}

void Window::deathNotify(Window *who) {
    if (parent)
        parent->deathNotify(who);
}

int Window::trueX() const {
    return parent_x + (parent ? parent->trueX() : 0);
}

int Window::trueY() const {
    return parent_y + (parent ? parent->trueY() : 0);
}

ProxyWindow::ProxyWindow(Window *_parent, int _w, int _h, int _x, int _y) : Window(_parent, _w, _h, None, _x, _y),
proxy_target(NULL) {
}

Border::Border(Window *_parent, int _w, int _h, int _x, int _y) : ProxyWindow(_parent, _w, _h, _x, _y) {
}

void Border::visibilityNotify(Window *w, bool visible) {
    if (w == proxy_target) {
        show(visible);
    }
}

void Border::deathNotify(Window *who) {
    if (who == proxy_target)
        die();
}

void ProxyWindow::insert(Window *w) {
    proxy_target = w ;
    Window::insert(w);
}
