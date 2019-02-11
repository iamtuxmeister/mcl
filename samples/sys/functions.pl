sub run {
    my $x = join(" ", @_);

    # Protect against accidents
    $x =~ s/;/\\;/;
    $x =~ s/\n/ /;
    $x = $x . "\n";
    syswrite INTERP, $x, length($x);
}

# This value is the file descriptor of the interpreter pipe and is set by
# mcl

open(INTERP, ">&$interpreterPipe") or die;

# Create wrappers for other mcl functions
foreach (qw/load open close reopen quit speedwalk bell echo status exec window kill print alias action send help eval run setinput clear prompt send_unbuffered chat chatall/) {
    eval "sub mcl_$_ { run (\"${commandCharacter}$_ \" . join(' ', \@_)); }";
}

1;
