%Gags = ();

sub cmd_gag {
    my ($count) = (0);
    if (/^(\d+)/) {
        # Remove a certain gag
        foreach (keys %Gags) {
            if (++$count == $1) {
                print "Deleted gag: $_\n";
                delete $Gags{$_};
                return;
            }
        }
        print "No such gag (there are only $count!) - type gag to see a list.\n";
    } elsif (/^.{3,}$/) {
        $Gags{$_} = 0;
        print "Gagging '$_' from now on.\n" unless $Loading;
    } else {
        print "Following gags are active:\n";
        foreach (keys %Gags) {
            printf "%2d) $_ (%d)\n", ++$count, $Gags{$_};
        }
        print "\nUse gag <string> to add a gag.\n";
        print "Use gag <number> to remove.\n";
    }
}

sub gag_input_hook {
    foreach $gag (keys %Gags) {
        if (/$gag/) {
            $Gags{$gag}++;
            $_ = "";
            return;
        }
    }
}

sub save_gags {
    local (*FH) = $_[0];
    foreach (keys %Gags) {
        print FH "Gag $_\n";
    }
}

output_add(\&gag_input_hook);
save_add(\&save_gags);
load_add("gag", \&cmd_gag);

