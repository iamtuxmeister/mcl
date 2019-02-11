# This utility function needs be located in this file
# Use this, not require directly!
sub include {
    eval "require \"$_[0]\"";
    print "include $_[0] => $@\n" if $@;
}
sub init {
    # Fallback to global installation
    push @INC, "$ENV{HOME}/.mcl", "/usr/local/lib/mcl", "/usr/lib/mcl";

	# mbt aug 00 -- hack to make things work under Debian (ugh)
	if (-f "/etc/debian_version") {
	  push @INC, "/usr/lib/perl5/5.005", "/usr/lib/perl5/5.005/i386-linux";
	}
    
    # This contains the code for the callout manager
    include "sys/hook.pl";      # Functions for managing hooks
    include "sys/functions.pl"; # Lots of utility functions
    include "sys/color.pl";     # Color code definitions
    include "sys/idle.pl";      # Callouts
    include "sys/config.pl";    # Configuration management
    include "sys/keys.pl";      # $keySomething definitions
    
    eval "require 'localinit.pl'"; # Optional
    
    
    # Do autoloading. Just use builtin glob, to reduce dependency
    # on Perl version
    my @AutoloadDirectories = ();
    if (-d "$ENV{HOME}/.mcl/auto") {
        push @AutoloadDirectories, "$ENV{HOME}/.mcl/auto";
    } else {
        push @AutoloadDirectories, "/usr/local/lib/mcl/auto", "/usr/lib/mcl/auto";
    }
    
    foreach $AutoDir (@AutoloadDirectories) {
        foreach (glob("$AutoDir/*.pl")) {
            #            print "Considering loading of: $_\n";
            require $_;
        }
        
        foreach (glob("$AutoDir/*")) {
            if (-d $_ and /\/([^\/]+)$/ and -f "$_/$1.pl") {
                require("$_/$1.pl");
            }
        }
    }

    &load_configuration();
    done_add(\&save_configuration);
}
