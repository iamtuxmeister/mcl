%Triggers = ();

sub trigscan {
    my ($t, $a);
    foreach $t (keys %Triggers) {
        next unless /$t/ && $Triggers{$t}->[1];
	foreach $a (split /;/, $Triggers{$t}->[0]) {
	    mcl_send($a);
	}
	last;
    }
}

sub cmd_trigger {
    my ($cmd, $trigstr, $trigact);
    
    ($cmd, $_) = split /\s+/, $_, 2;
    $cmd = "" unless defined $cmd;
    $_ = "" unless defined $_;
    if ($cmd eq "help") {
	trigger_help();
    } elsif ($cmd eq "" || $cmd eq "list") {
	if ($cmd eq "") {
	    print "For help, type: trigger help\n";
	}
	trigger_list();
    } elsif ($cmd eq "delete" || $cmd eq "enable" || $cmd eq "disable"
	     || $cmd eq "action") {
	my $i = 0;
	unless (/^(\d+).*/) {
	    print "trigger: $cmd: invalid argument\n";
	    return;
	}
	foreach $t (keys %Triggers) {
	    next unless ++$i == $1;
	    if ($cmd eq "delete") {
		print "trigger: deleting trigger $i\n";
		delete $Triggers{$t};
	    } elsif ($cmd eq "enable") {
		print "trigger: enabling trigger $i\n";
		$Triggers{$t}->[1] = 1;
	    } elsif ($cmd eq "disable") {
		print "trigger: disabling trigger $i\n";
		$Triggers{$t}->[1] = 0;
	    } elsif ($cmd eq "action") {
		if (/^(\d+)\s+(.*)/) {
		    $Triggers{$t}->[0] = $2;
		    print "trigger: redefined action of trigger $i to: $2\n";
		} else {
		    print "trigger: action: invalid syntax\n";
		}
	    }
	    return;
	}
	print "trigger: $cmd: $1: no such trigger\n";
    } elsif ($cmd eq "add" || $cmd eq "add_dis") {
	if (/^("([^\\"]*(\\.|"))*)\s+(.*)/) {
	    $trigstr = $1;
	    $trigact = $4;
	    $trigstr =~ s/.//;
	    $trigstr =~ s/.$//;
	    $trigstr =~ s/\\"/"/g;
	} elsif (/^(\S+)\s+(.*)/) {
	    $trigstr = $1;
	    $trigact = $2;
	} else {
	    print "trigger: add: invalid syntax\n";
	    return;
	}
	unless ($trigact ne "") {
	    print "trigger: add: empty action string\n";
	    return;
	}
	$Triggers{$trigstr} = [ $trigact, 1 ];
	$Triggers{$trigstr}->[1] = 0 if $cmd eq "add_dis";
	trigger_list($trigstr) unless $Loading;
    } else {
	print "trigger: invalid command\n";
	trigger_help();
    }
}

sub trigger_list {
    my $t = shift;
    my $i = 0;
    my $ts;
    $t = "" unless defined $t;
    foreach (keys %Triggers) {
	if (++$i == 1 && $t eq "") {
	    print "Currently defined triggers:\n";
	}
	next unless $t eq "" || $_ eq $t;
	print "*" unless $Triggers{$_}->[1];
	($ts = $_) =~ s/"/\\"/g;
	$ts = "\"$_\"";
	printf "%2d) $ts $Triggers{$_}->[0]\n", $i;
    }
    print "No triggers defined.\n" if $i == 0;
}

sub trigger_help {
    print <<EOF;
Usage: trigger [<command> [<arguments>]]

Valid commands are:
add <regexp> <action>	If the <regexp> is matched in an input line, <action>
			string is sent to the MUD.  If <regexp> must contain
			blanks, enclose it in double quotes ("").  Double quote
			itself must be preceded by a backslash if it is to be
			included in a quoted string.  <action> should not be
			quoted.

list			Display currently defined triggers.  Each trigger is
			preceded by its number, which is used to reference it
			in other commands.  Disabled triggers are printed with
			an asterisk (*) before the number.  An empty command
			also lists the triggers.

action <number> <newac>	Redefine the action of trigger <number> to <newac>.

delete <number>		Delete trigger <number>.

disable <number>	Disable trigger <number>.

enable <number>		Enable trigger <number>.
EOF
}

sub save_triggers {
    local (*TH) = $_[0];
    my ($t, $cmd, $trigact);
    foreach $t (keys %Triggers) {
	$trigact = $Triggers{$t}->[0];
	$cmd = $Triggers{$t}->[1] ? "add" : "add_dis";
	$t =~ s/"/\\"/g;
	$t = "\"$t\"";
        print TH "trigger ", $cmd, " ", $t, " ", $trigact, "\n";
    }
}

save_add(\&save_triggers);
load_add("trigger", \&cmd_trigger);
output_add(\&trigscan);

1;
