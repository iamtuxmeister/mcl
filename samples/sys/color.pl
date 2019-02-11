# Color definitions

$ColorCode = "\xEA";

{
    my $C = "\xEA";
    $SoftCR = "\xEB";
    
    $CSave = "\xEA\xFF";
    $CRestore = "\xEA\xFE";
    
    @Colors = qw/Black Blue Green Cyan Red Magenta Yellow White/;
    
    my $x;
    
    foreach $x (0 .. $#Colors) {
        $ColorVal{$Colors[$x]} = $x;
        eval "\$$Colors[$x] = \"${C}" . chr($x) . "\";" or print "ERROR: $@\n";
        eval "\$B$Colors[$x] = \"${C}" . (chr($x+8)) . "\";" or print "ERROR: $@\n";
        
    }

    $Off = $White; # Alias
}

# Set both foreground and background color
sub setColor ($$) {
    my ($fg, $bg) = @_;
    return "\xEA" . chr(ord(substr($fg, 1, 1)) + 16 * ord (substr($bg, 1,1)));
}

# bold_green, bold_green_red etc.
sub str_to_color {
    my ($str,$fg,$bg) = ($_[0],0,0);
    $str =~ s/((^|_)\w)/uc $1/ge;
    if ($str =~ /^(bold_)?([^_]+)_?(.*)/i) {
        $fg = $ColorVal{$2} if exists $ColorVal{$2};
        $bg = $ColorVal{$3} if exists $ColorVal{$3};
        $fg += 8 if length($1);
        return ("\xEA" . chr($fg + ($bg << 4)));
    }
    else {
        return "";
    }
}

# The other way around
sub color_to_str {
    my ($col,$val,$bold,$fg,$bg) = ($_[0], "", "", "", "");
    if (substr($col,0,1) eq "\xEA") {
        $val = ord(substr($col, 1,1));
        $bold = "bold_" if $val & 8;
        $fg = $Colors[$val & 7];
        $bg = "_" . $Colors[($val>>4) & 7];
        $bg = "" if $bg eq "Black";

        return "$bold$fg$bg";
    }
    return "";
}

# Example of use:
# print "Loaded the ${BRed}C${Green}O${BBlue}L${Cyan}O${Magenta}U${BYellow}R${White} module.\n";
# print "Let's try some", setColor($White, $Green), " background${White} color. \n";

1;
