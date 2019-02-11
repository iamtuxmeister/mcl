# Tab completion module

# The length of the last completion, e.g. dest<tab>roy will have $LastLen=3
# so another TAB after that means that we should go that long back
# to look for a word
$LastCompletionLen = 0;
$PrevIndex = 0;  # Which completion did we pick the last time?
$CompletionRecursing = 0;
%Completions = ( "complete" => 1, "autocomplete" => 1 );

# Autocompletion parameters.
@Volatile = ();
$AutocompleteMin = 5;
$AutocompleteSize = 200;
$AutocompleteSmashCase = 0;

sub tab_complete {
    my ($word,$complete,$n);
    if ($Key eq $keyTab) {
        # Try tab completion
        ($word) = /(\w*).{$LastCompletionLen}$/;
        foreach $complete (keys %Completions, reverse @Volatile) {
            if ($complete =~ /^$word/i and $n++ == $PrevIndex) {
                # Automatically stick in a space here ?
                s/\w*.{$LastCompletionLen}$/$complete /;
                $LastCompletionLen = (1+length($complete)) - length($word);
                $Key = 0;
                $PrevIndex = $n;
                return;
            }
        }

        # End of list but we have completed something before? Try recursing
        # But don't recurse if we only have one possible item
        if ($LastCompletionLen and !$CompletionRecursing and $PrevIndex > 1) {
            $CompletionRecursing = 1;
            $PrevIndex = 0;
            tab_complete();
            $CompletionRecursing = 0;
        } else {
            mcl_bell();
            $Key = 0;
        }
    } elsif ($Key eq $keyBackspace and $LastCompletionLen) { # Backspace deletes completed word
        s/\w*.{$LastCompletionLen}\s*$//;
        $LastCompletionLen = 0;
        $PrevIndex = 0;
        $Key = 0;
    }
    else {
        $LastCompletionLen = 0;
        $PrevIndex = 0;
    }
}

sub cmd_complete {
    my ($count);

    if (/^(\d+)$/) {
        $cout = 0;
        foreach (keys %Completions) {
            if (++$count == $1) {
                print "Deleted completion #$1: $_\n";
                delete $Completions{$_};
                return;
            }
        }
        print "There are only $count completions. Type completions to see.\n";
    }
    elsif (length > 2) {
        # Add completion
        if (exists $Completions{$_}) {
            print "Completion $_ already exists.\n" unless $Loading;
        } else {
            $Completions{$_}++;
            print "Added completion for '$_'\n" unless $Loading;
        }
    } elsif (length == 0) {
        print "Completions:\n";
        $count = 1;
        foreach (keys %Completions) {
            printf "%3d) $_\n", $count++;
        }
    } else {
        print "A completion must be at least 2 characters long. Type completions to list existing ones.\n";
    }
}

sub load_completions {
    foreach(split (',', $_)) {
        &cmd_complete;
    }
}

sub save_completions {
    local (*FH) = $_[0];
    if (scalar(keys %Completions)) {
        print FH "completions ", join(',', keys %Completions), "\n";
    }
    print FH "autocomplete min ", $AutocompleteMin, "\n";
    print FH "autocomplete size ", $AutocompleteSize, "\n";
    print FH "autocomplete smashcase ", $AutocompleteSmashCase ? "on" : "off",
	     "\n";
}

sub cmd_autocomplete {
    my ($cmd, $val);
    if (/^(\S+)\s+(\d+).*/) {
	$cmd = $1;
	$val = $2;
	if ($cmd eq "min") {
	    $val = 3 if $val < 3;
	    $AutocompleteMin = $val;
	} elsif ($cmd eq "size") {
	    $val = 100 if $val < 100;
	    $AutocompleteSize = $val;
	} else {
	    print "autocomplete: unknown command\n";
	    autocomplete_help();
	}
    } elsif (/^smashcase\s+(on|off).*/) {
	$AutocompleteSmashCase = $1 eq "on";
    } elsif ($_ eq "param" || $_ eq "") {
	if ($_ eq "") {
	    print "For help, type: autocomplete help\n";
	}
	print <<EOF;
Current autocompletion parameters:
   Minimum word size: $AutocompleteMin
Maximum history size: $AutocompleteSize
EOF
	print " Lowercase all input: ", $AutocompleteSmashCase ? "on" : "off",
	      "\n";
    } elsif ($_ eq "help") {
	autocomplete_help();
    } else {
	print "autocomplete: invalid syntax\n";
    }
}

sub volatile_completions {
    my (@w, %w, $w, @v);
    my ($i, $n, $l);

    $l = $_;
    $_ = lc $l if $AutocompleteSmashCase;
    @w = /(\w{$AutocompleteMin,})/g;
    foreach $w (@w) {
	$w{$w} = 1;
    }
    while ($#Volatile > $AutocompleteSize - $#w - 1) {
	shift @Volatile;
    }
    for ($n = 0, $i = 0; $i <= $#Volatile; $i++) {
	next if defined $w{$Volatile[$i]};
	$v[$n++] = $Volatile[$i];
    }
    %w = ();
    for ($i = 0; $i <= $#w; $i++) {
	next if defined $w{$w[$i]};
	$w{$w[$i]} = 1;
	$v[$n++] = $w[$i];
    }
    @Volatile = @v;
    $_ = $l;
}

sub autocomplete_help {
    print <<EOF;
Usage: autocomplete <command> [<argument>]

Valid commands are:
min <number>		Minimum word length for inclusion in the autocompletion
			array (default 5).

size <number>		Maximum size of the autocompletion array (default 200).

smashcase {on|off}	Convert all input to lowercase (default off).

param			Display current autocompletion parameters.
EOF
}

save_add(\&save_completions);
load_add("completions", \&load_completions);
keypress_add(\&tab_complete);

load_add("autocomplete", \&cmd_autocomplete);
output_add(\&volatile_completions);

1;
