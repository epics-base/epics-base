eval 'exec perl -S $0 ${1+"$@"}'  # -*- Mode: perl -*-
    if $running_under_some_shell; # replaceVAR.pl

# Called from within the object directory
# Replaces VAR(xxx) with $(xxx)
# and      VAR_xxx_ with $(xxx)

while (<STDIN>) {
    s/VAR\(/\$\(/g;
    s/VAR_([^_]*)_/\$\($1\)/g;
    print;
}
