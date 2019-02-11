// Output window implementation

#include "mcl.h"
#include "cui.h"
#include "InputBox.h"
#include "ScrollbackSearch.h"
#include <time.h>

OutputWindow::OutputWindow(Window *_parent)
: Window(_parent, wh_full, _parent->height-1), top_line(0)
{
	// Forget the canvas
	delete[] canvas;

    highlight.line = -1;
    
	scrollback = new attrib[width * config->getOption(opt_scrollback_lines)];
	viewpoint = canvas = scrollback; // point at the beginning
	fFrozen = false;
    clear();
    screen->setScrollingRegion(trueX(), trueY(), trueX() + width, trueY() + height);
}

OutputWindow::~OutputWindow()
{
	canvas = NULL; // Make sure noone delets this
	delete[] scrollback;
}

#define COPY_LINES 250	

// 'scroll' one line up
bool OutputWindow::scroll()
{
	if (canvas == scrollback + width * (config->getOption(opt_scrollback_lines) - height))
	{
		// We're at the end of our buffer. Copy some lines up
		memmove(scrollback, scrollback + width * COPY_LINES, 
			(config->getOption(opt_scrollback_lines) - COPY_LINES) * width * sizeof(attrib));
		canvas -= width * COPY_LINES;
        viewpoint -= width * COPY_LINES;

        top_line += COPY_LINES;

		if (viewpoint < scrollback)
			viewpoint = scrollback;
		
		// OK, now clear one full screen beneath
		canvas += width * height; // Cheat :)
		clear();
		canvas -= width * height;
	}
	else
	{
		canvas += width; // advance the canvas
		clear_line(height-1); // Make sure the bottom line is cleared
		cursor_y--;
	
		if (!fFrozen) // Scroll what we are viewing, too
			viewpoint += width;
	}

	return true;
}
bool OutputWindow::moveViewpoint(move_t dir)
{
	int amount;
	bool fQuit = false;
	
	switch (dir)
	{
		case move_page_down: // page size
			amount = height/2;
			break;
		
		case move_page_up:
			amount = - height/2;
			break;
		
		case move_home:
			amount = - (viewpoint-scrollback)/width;
			break;
		
		case move_line_up:
			amount = -1;
			break;
		
		case move_line_down:
			amount =  1;
			break;

		case move_stay:		
			amount = 0;
			break;
		
		default: // uuh.
			abort();
	}
	
	if (amount < 0 && viewpoint == scrollback)	
		status->setf ("You are already at the beginning of the scrollback buffer");
	else if (amount > 0 && viewpoint == canvas)
		fQuit = true; // Leave
	else
	{
		viewpoint += amount * width;
		if (viewpoint < scrollback)
			viewpoint = scrollback;
		
		if (viewpoint > canvas)
			viewpoint = canvas;
		status->sticky_status = true;
		status->setf("Scrollback: line %d of %d",
			(viewpoint - scrollback) / width,
			(canvas-scrollback) / width + height);
	}
	dirty = true;

	return fQuit;
}


// This one handles a select number of keys
bool ScrollbackController::keypress(int key)
{
    bool fQuit;
    
    switch (key)
    {
    default:
        return false;
        
    case key_page_up: // Scroll one page up
        fQuit = output->moveViewpoint(OutputWindow::move_page_up);
        break;
        
    case key_page_down:
        fQuit = output->moveViewpoint(OutputWindow::move_page_down);
        break;
        
    case key_arrow_up:
        fQuit = output->moveViewpoint(OutputWindow::move_line_up);
        break;
        
    case key_arrow_down:
        fQuit = output->moveViewpoint(OutputWindow::move_line_down);
        break;
        
    case key_home:
        fQuit = output->moveViewpoint(OutputWindow::move_home);
        break;
        
    case key_pause:
        fQuit = output->moveViewpoint(OutputWindow::move_stay);
        break;
        
    case key_end:
        fQuit = true;
        break;
    }
    
    
    if (fQuit)
    {
        status->sticky_status = false;
        status->setf ("Leaving scrollback.");
        output->unfreeze();
        die();
    }
    
    return true;
}

void OutputWindow::search (const char *s, bool forward)
{
    attrib *p = viewpoint + (width * (cursor_y-1)), *q, *r;
    const char *t;
    int len = strlen(s);
    bool found = true;
    
    for (;;)
    {
        // Search current line first
        // Start search at beginning and continue until <len> offset from the right margin
        for (q = p; q < p + width - len; q++)
        {
            found = true;
            // Now compare the letters
            for (r = q, t = s; *t; t++, r++)
            {
                if ( tolower((*r & 0xFF)) != tolower(*t))
                {
                    found = false;
                    break;
                }
            }

            if (found)
                break;

        }
        
        if (found)
            break;
        
        // Go forward or backward
        if (forward)
        {
            if (p == canvas + width * cursor_y) // we're searching the very last line!
                break;
            
            p += width;
        }
        else
        {
            if (p == scrollback)
                break;
            
            p -= width;
        }
    }
    
    if (!found)
        status->setf ("Search string '%s' not found", s);
    else
    {
        highlight.line = ((p-scrollback) / width) + top_line;
        highlight.x = q - p;
        highlight.len = strlen(s);
        
        // Show on the second line rather than under status bar
        viewpoint = scrollback >? p-width;
        viewpoint = viewpoint <? canvas;
        status->setf("Found string '%s'", s);
    }
}


void OutputWindow::draw_on_parent()
{
    // Show what we are viewing (not necessarily the same as what we are drawing on
    if (parent)
    {
        
        // Now we need to highlight search strings, if any
        if (highlight.line >= 0 &&
            highlight.line-top_line >= (viewpoint-scrollback) / width &&
            highlight.line-top_line <= ((viewpoint-scrollback) / width + height))
        {
            attrib *a, *b;
            a = viewpoint + width * ( (highlight.line - top_line) - (viewpoint-scrollback)/width) + highlight.x;

            // Copy the old stuff into a temp array ,ugh
            // This seems to be the easiest solution however
            attrib old[highlight.len];
            memcpy(old, a, highlight.len*sizeof(attrib));
            for (b = a; b < a+highlight.len; b++)
            {
                unsigned char color = ((*b & 0xFF00) >> 8) & ~(fg_bold|fg_blink);
                unsigned char bg = (color & 0x0F) << 4;
                unsigned char fg = (color & 0xF0) >> 4;

                *b = (*b & 0x00FF) | ((bg|fg) << 8);
            }

            parent->copy (viewpoint, width,height,parent_x,parent_y);

            memcpy(a, old, highlight.len*sizeof(attrib));
            
        }
        else
            parent->copy (viewpoint, width,height,parent_x,parent_y);
        
    }
}

void OutputWindow::move (int x, int y) {
    Window::move(x,y);
    screen->setScrollingRegion(trueX(), trueY(), trueX() + width, trueY() + height);
}

// Print some version information
void OutputWindow::printVersion()
{
    printf (
            "\n\n"
            "mcl version %s\n"
            "Copyright (C) 1997-2000 Erwin S. Andreasen <erwin@andreasen.org>\n"
            "mcl comes with ABSOLUTELY NO WARRANTY; for details, see file COPYING\n"
            "This binary compiled: " __DATE__ ", " __TIME__ " by " COMPILED_BY ".\n"
            "Binary/source available from http://www.andreasen.org/mcl/\n",
            versionToString(VERSION)
           );

    if (screen->isVirtual())
        printf("Using fast Virtual Console output routines\n");
    else
        printf("Using slow TTY output routines\n");
}

void OutputWindow::saveToFile (const char *fname, bool use_color) {
    FILE *fp = fopen(fname, "w");
    if (!fp)
        status->setf("Cannot open %s for writing: %s", fname, strerror(errno));
    else {
        fprintf(fp, "Scrollback saved from mcl %s at %s", versionToString(VERSION), ctime(&current_time));

        int color = -1;
        for (attrib *line = scrollback; line < canvas + (width * height); line += width) {
            for (attrib *a = line; a < line+width; a++) {
                if (use_color && (*a >> 8) != color) {
                    fputs(screen->getColorCode(*a >> 8, true), fp);
                    color = *a >> 8;
                }
                fputc(*a & 0xFF, fp);
            }
            fputc('\n', fp);
        }
        fclose(fp);
        status->setf("Scrollback saved successfully");
    }
}

void ScrollbackSearch::execute(const char *text)
{
    ScrollbackController *s;
    
    if (text[0]) {
        // If the scrollback is not up yet, we need to create it
        if (!( s = (ScrollbackController*) (screen->findByName ("ScrollbackController"))))
            (s = new ScrollbackController(screen, output))->keypress(key_pause);
        
        output->search(text, forward);
    }

    die();
}


