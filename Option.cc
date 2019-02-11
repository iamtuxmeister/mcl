
#include "mcl.h"
#include "Option.h"


OptionParser::OptionParser(const char *_s, const char *_options)
    :  s(_s), options(_options)
{
    memset(option_table,0,sizeof(option_table));

    // parse a:bcd:: so that e.g. option_table['a'] = optionalArgument
    while(*_options)
    {
        int c = (unsigned char) *_options;
        int count = 0;

        while(*(_options+1) == ':')
        {
            count++;
            _options++;
        }

        switch(count)
        {
            case 0:
            option_table[c] = noArgument;
            break;

            case 1:
            option_table[c] = optionalArgument;
            break;

            default:
            option_table[c] = requiredArgument;
        }

        _options++;
    }
}

const char * one_argument (const char *s, char *arg)
{
    while(isspace(*s))
        s++;

    while(*s && !isspace(*s))
        *arg++ = *s++;

    *arg = NUL;

    while(isspace(*s))
        s++;

    return s;
}

int OptionParser::nextOption(String &args)
{
    char arg[256];
    String previous;
    int previous_option = -1;
    

    for (;;)
    {
        char temp[512];
        previous = s;

        // BARF
        strcpy(temp, one_argument(s, arg));
        s = temp;

        if (previous_option != -1)
        {
            if (option_table[previous_option] == requiredArgument && !arg[0])
            {
                args.printf("Option -%c required an argument", previous_option);
                return invalidOption;
            }

            if (arg[0] != '-' || option_table[previous_option] != optionalArgument)
            {
                args = arg;
                return previous_option;
            }
            else // Following is another option, backup
            {
                args = "";
                s = previous;
                return previous_option;
            }
        }

        // Not an option anymore
        // return what's left in args
        if (arg[0] != '-')
        {
            args = previous;
            return 0;
        }

        // No more options, the rest is data
        if (!strcasecmp(arg, "--"))
        {
            args = s;
            return 0;
        }

        if(!strcasecmp(arg, "-") || option_table[arg[1]] == optionNotSet)
        {
            args.printf("Invalid option: %s", arg);
            return invalidOption;
        }

        if (option_table[arg[1]] == noArgument)
        {
            if (strlen(arg) > 2)
            {
                args.printf("Option -%c takes no parameters but called with %s", arg[1], arg);
                return invalidOption;
            }
            else
                return arg[1];
        }

        // Arg given directly afterwards (-vfoo)
        if (strlen(arg) > 2)
        {
            args = arg+2;
            return arg[1];
        }

        previous_option = arg[1];
    }
}

