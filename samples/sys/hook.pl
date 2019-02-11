# Functions for managing a table of hooks

# hook_add(table, key, data)
sub hook_add ($$$) {
   my ($table, $key, $data) = @_;
   $$table{$key} = $data;
}

sub hook_remove ($$) {
    my ($table, $data, $x) = @_;
    
    foreach $x (keys %$table) {
        if ($$table{$x} eq $data)   {
            delete $${table}{$x};
            return;
        }
    }
}

sub hook_run ($) {
    my ($table, $sub, $n) = @_;
    foreach $sub (keys %$table) {
        $n = $$table{$sub};
        &$n();
    }
}

# create standard hook functions for those. if $want_run is set, create
# a run function as well
sub create_standard_hooks($@) {
   my ($want_run,@hooks, $name,$x) = (@_);
   foreach $name (@hooks) {
       $x = "sub ${name}_add { my(\$data,\$key) = (\@_,\$lastHook++); hook_add(\\\%${name}_hooks, \$key, \$data); };\n" .
       "sub ${name}_remove (\$) { hook_remove(\\\%${name}_hooks, \$_[0]); };\n";


   $x .= "sub ${name} { hook_run(\\\%${name}_hooks); };\n" if $want_run;
           ;
   #   print "Evaluating:\n$x\n";
   eval $x; print "ERROR: $@\n" if $@;
   }
}

sub command {
    my ($temp);
    if (/^(\w+)\s*(.*)/ and exists $command_hooks{lc $1}) {
        $temp = $_; $_ = $2;
        if (&{$command_hooks{lc $1}}) { # Stop
            $_ = "";
        } else {
            $_ = $temp; # OK.
        }
    }
}

# The hooks themselves
@Hooks = qw/send preinput loselink prompt userinput postoutput output done keypress connect/;
&create_standard_hooks(1, @Hooks);
&create_standard_hooks(0, qw/command/);

# Debug mostly command
sub cmd_show_hooks {
    my ($t, $h);
    foreach $t (@Hooks) {
        print "Hooks of type $t:\n";
        eval "foreach \$h (keys %${t}_hooks) { print \"\$${t}_hooks{\$h}\n\";}";
    }
}

1;
