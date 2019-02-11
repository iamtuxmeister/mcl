#include "Control.h"
class Hotkey : Control {
public:
    Hotkey(Window *_parent) : Control(_parent) {}
    virtual bool  keypress (int key);
};

extern bool mclRestart;
