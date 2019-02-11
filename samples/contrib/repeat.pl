# Repeat the last command entered when just enter is pressed

sub check_repeat {
        if ($_ eq "") {
                $_ = $LastCommandEntered;
        } else {
                $LastCommandEntered = $_;
        }
}

userinput_add(\&check_repeat);
