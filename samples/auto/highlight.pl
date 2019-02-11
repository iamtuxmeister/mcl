%Highlights = ();

# We use the color save/restore codes here
sub highlighter {
    foreach $p (keys %Highlights) {
        s/($p)/${CSave}$Highlights{$p}$1${CRestore}/g;
    }
}

sub cmd_highlight {
    my ($col,$Color,$What,$count);
    
    if (/^(\S+)\s+(.*)/) { # Display highlights
        $Color = $1; $What = $2;
        if (length($col = &str_to_color($Color))) {
            $Highlights{$What} = $col;
            print "Highlighting: ${col}${What}${White}\n" unless $Loading;
        } else {
            print "'$Color' is not a valid color code.\n";
        }
    } elsif (/^(\d+)/) {
        # Remove a certain highlight
        $count = 0;
        foreach (keys %Highlights) {
            if (++$count == $1) {
                print "Deleted: $Highlights{$_}$_${White}\n";
                delete $Highlights{$_};
                return;
            }
        }
        print "No such highlight (there are only $count!) - type highlight to see a list.\n";
        
    } else {
        print "Following highlights are active:\n";
        foreach (keys %Highlights) {
            printf "%2d) $Highlights{$_}$_${White}\n", ++$count;
        }
        print "\nUse highlight <color> <string> to highlight.\n";
        print "Use highlight <number> to remove.\n";
    }
}

sub save_highlights {
    local (*FH) = $_[0];
    foreach (keys %Highlights) {
        print FH "highlight ", &color_to_str($Highlights{$_}), " $_\n";
    }
}

save_add(\&save_highlights);
load_add("highlight", \&cmd_highlight);
output_add(\&highlighter);


1;
