# Configuration manager

# Change if necessary
$CONFIG_FILE = "$ENV{HOME}/.mcl/config.perl";
$OLD_CONFIG_FILE = "$ENV{HOME}/.mcl/config";

# Initial loading of configuration
# Must be done after all auto/* modules have had a chance to run...
sub load_configuration {
    $Loading = 1;
    if (open(CONFIG, $CONFIG_FILE) || open(CONFIG, $OLD_CONFIG_FILE)) {
        while (<CONFIG>) {
            chomp;
            next if /^#/;
            if (/^(\w+)\s*(.*)/) {
                if ($Loaders{lc $1}) {
                    $_ = $2;
                    &{$Loaders{lc $1}};
                } else {
                    print "Unsupported keyword ($1) on line $. of $CONFIG_FILE\n";
                    print "File is now read-only to allow you to decidex what to do with the lines.\n" unless $ReadOnly;
                    $ReadOnly = 1;
                }
            }
        }
        close (CONFIG);
    }

    $Loading = 0;
}

sub save_configuration {
    return if $ReadOnly;
    
    open(CONFIG, ">" . $CONFIG_FILE) or die "Could not open $CONFIG_FILE for writing: $!";
    print CONFIG "# mcl module config file generated on ", scalar(localtime($now)), " by $VERSION\n";
    
    foreach (@Savers) {
        &$_ (*CONFIG);
    }
    close (CONFIG);
}

sub load_add ($$) {
    $Loaders{$_[0]} = $_[1];
}

sub save_add ($) {
    push @Savers, $_[0];
}

