
#include <limits.h>

/*
 This parses an option string, much like getopt()
 Usage:
 OptionParser o("-v -y", "vy");
 String args;
 int c;
 
 while ((c = o.nextOption(args)))
 switch (c)
 {
 case 'v':
 
 
 case 'y':
 
 default:
 print ("Unknown option: %c\n", c);
 }

 printf ("Leftover options: %s\n", (const char *) args);
 
 */

class OptionParser
{
public:
    OptionParser(const char *_s, const char *_options);
    int nextOption(String &args);

    // result codes
    static const int invalidOption = -1;
    
private:
    String s;
    String options;

    typedef enum {optionNotSet, noArgument, optionalArgument, requiredArgument} arg_t;
    arg_t option_table[UCHAR_MAX+1];
};

