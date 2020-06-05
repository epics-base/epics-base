######################################################################
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement
# found in file LICENSE that is included with this distribution.
#
#    Original Author: Shantha Condamoor, SLAC
#    Creation Date: 1-Sep-2011
#    Current Author: Andrew Johnson

use strict;
use warnings;

package EPICS::IOC;
require 5.010;

=head1 NAME

EPICS::IOC - Manage an EPICS IOC

=head1 SYNOPSIS

    use EPICS::IOC;

    my $ioc = EPICS::IOC->new;
    $ioc->debug(1);       # Show IOC stdio streams

    $ioc->start('bin/@ARCH@/ioc', 'iocBoot/ioc/st.cmd');
    $ioc->cmd;            # Wait for the iocsh prompt

    my @records = $ioc->dbl;
    my @values = map { $ioc->dbgf($_); } @records;

    $ioc->exit;

=head1 DESCRIPTION

This module provides an object-oriented API for starting, interacting with and
stopping one or more EPICS IOCs under program control, and is generally intended
for writing test programs.

The IOC should not be configured to emit unsolicited messages on stdout as this
could interfere with the ability of the software to detect an end-of-command
from the IOC shell, which is achieved by setting the prompt to a known string
(normally C<__END__>). Unsolicited messages on stderr will not cause problems,
but can't be seen on Windows systems.

=head1 CONSTRUCTOR

=over 4

=cut

use Symbol 'gensym';
use IPC::Open3;
use IO::Select;


=item new ()

Calling C<new> creates an C<EPICS::IOC> object that can be used to start and
interact with a single IOC. After this IOC has been shut down (by calling its
C<exit> or C<close> methods) the C<EPICS::IOC> object may be reused for another
IOC.

=back

=cut

sub new {
    my $proto = shift;
    my $class = ref $proto || $proto;

    my $self  = {
        pid => undef,
        stdin => gensym,
        stdout => gensym,
        stderr => gensym,
        select => IO::Select->new(),
        errbuf => '',
        debug => 0,
        terminator => '__END__'
    };

    bless $self, $class;
}

=head1 METHODS

=over 4

=item debug ( [FLAG] )

Each C<EPICS::IOC> object has its own debug flag which when non-zero causes all
IOC console traffic sent or read by other methods to be printed to stdout along
with the IOC's pid and a direction indicator. The C<debug> method optionally
sets and returns the value of this flag.

The optional FLAG is treated as a true/false value. If provided this sets the
debug flag value.

The method's return value is the current (new if given) value of the flag.

=cut

sub debug {
    my $self = shift;

    $self->{debug} = shift if scalar @_;
    return $self->{debug};
}


=item start ( EXECUTABLE [, ARGUMENTS ...] )

Launch an IOC binary given by EXECUTABLE with ARGUMENTS. The method dies if it
can't run the program as given, or if the IOC is already running.

In most cases the C<cmd> method should be called next with no command string,
which waits for the IOC's boot process to finish and the first iocsh prompt to
be displayed.

The C<start> method sets two environment variables that control how the IOC
shell behaves: C<IOCSH_HISTEDIT_DISABLE> is set to prevent it calling the GNU
Readline library, and C<IOCSH_PS1> is set to a known string which is used as a
terminator for the previous command.

=cut

sub start {
    my ($self, $exe, @args) = @_;

    croak("IOC already running") if $self->started;

    # Turn off readline or its equivalents
    local $ENV{IOCSH_HISTEDIT_DISABLE} = "TRUE";

    # The iocsh prompt marks the end of the previous command
    local $ENV{IOCSH_PS1} = $self->{terminator} . "\n";

    # Run the IOC as a child process
    $self->{pid} = open3($self->{stdin}, $self->{stdout}, $self->{stderr},
        $exe, @args)
        or die "can't start $exe: $!";

    $self->{select}->add($self->{stderr});

    printf "#%d running %s\n", $self->{pid}, $exe if $self->{debug};
}


=item pid ()

Returns the process-ID of the IOC process, or undef if the IOC process has not
yet been started.

=cut

sub pid {
    my $self = shift;

    return $self->{pid};
}


=item started ()

Returns a true value if the IOC has been started and not yet closed. This state
will not change if the IOC dies by itself, it indicates that the C<start>
method has been called but not the C<close> method.

=cut

sub started {
    my $self = shift;

    return defined($self->pid);
}


=item _send ( COMMAND )

The C<_send> method is a primitive for internal use that sends a COMMAND string
to the IOC shell, and prints it to stdout if the debug flag is set.

=cut

sub _send {
    my ($self, $cmd) = @_;
    my $stdin = $self->{stdin};

    printf "#%d << %s", $self->{pid}, $cmd if $self->{debug};

    local $SIG{PIPE} = sub {
        printf "#%d << <PIPE>\n", $self->{pid} if $self->{debug};
    };

    print $stdin $cmd;
}


=item _getline ()

The C<_getline> method is also designed for internal use, it fetches a single
line output by the IOC, and prints it to stdout if the debug flag is set.

Any CR/LF is stripped from the line before returning it. If the stream gets
closed because the IOC shuts down an C<EOF> debug message may be shown and an
undef value will be returned.

=cut

sub _getline {
    my $self = shift;
    my $pid = $self->pid;
    return undef
        unless $self->started;

    # Save, could be closed by a timeout during readline
    my $stdout = $self->{stdout};
    my $debug = $self->{debug};

    my $line = readline $stdout;
    if (defined $line) {
        $line =~ s/[\r\n]+ $//x;    # chomp broken on Windows?
        printf "#%d >> %s\n", $pid, $line if $debug;
    }
    elsif (eof($stdout)) {
        printf "#%d >> <EOF>\n", $pid if $debug;
    }
    else {
        printf "#%d Error: %s\n", $pid, $! if $debug;
    }
    return $line;
}


=item _getlines ( [TERM] )

Another internal method C<_getlines> fetches multiple lines from the IOC. It
takes an optional TERM string or regexp parameter which is matched against each
input line in turn to determine when the IOC's output has been completed.
Termination also occurs on an EOF from the output stream.

The return value is a list of all the lines received (with the final CR/LF
stripped) including the line that matched the terminator.

=cut

sub _getlines {
    my ($self, $term) = @_;

    my @response = ();

    while (my $line = $self->_getline) {
        push @response, $line;
        last if defined $term && $line =~ $term;
    }
    return @response;
}


=item _geterrors ( )

Returns a list of lines output by the IOC to stderr since last called. Only
complete lines are included, with trailing newline char's removed.

NOTE: This doesn't work on Windows because it uses select which Perl doesn't
support on that OS, but it doesn't seem to cause any problems for short-lived
IOCs at least, it just never returns any text from the IOC's stderr output.

=cut

sub _geterrors {
    my ($self) = @_;
    my @errors;

    while ($self->{select}->can_read(0.01)) {
        my $n = sysread $self->{stderr}, my $errbuf, 1024;
        return @errors unless $n;   # $n is 0 on EOF
        push @errors, split m/\n/, $self->{errbuf} . $errbuf, -1;
        last unless @errors;
        $self->{errbuf} = pop @errors;
    }
    return @errors;
}

=item cmd ( [COMMAND [, ARGUMENTS ...]] )

If the C<cmd> method is given an optional COMMAND string along with any number
of ARGUMENTS it constructs a command-line, quoting each argument as necessary.
This is sent to the IOC and a line read back and discarded if it matches the
command-line.

With no COMMAND string the method starts here; it then collects lines from the
IOC until one matches the terminator. A list of all the lines received prior to
the terminator line is returned.

=cut

sub cmd {
    my ($self, $cmd, @args) = @_;

    my @response;
    my $term = $self->{terminator};

    if (defined $cmd) {
        if (@args) {
            # FIXME: This quoting stuff isn't quite right
            my @qargs = map {
                m/^ (?: -? [0-9.eE+\-]+ ) | (?: " .* " ) $/x ? $_ : "'$_'"
            } @args;
            $cmd .= ' ' . join(' ', @qargs);
        }
        $self->_send("$cmd\n");

        my $echo = $self->_getline;
        return @response unless defined $echo;    # undef => reached EOF
        if ($echo ne $cmd) {
            return @response if $echo =~ $term;
            push @response, $echo;
        }
    }

    push @response, $self->_getlines($term);
    pop @response if @response and $response[-1] =~ $term;

    my @errors = $self->_geterrors;
    if (scalar @errors && $self->{debug}) {
        my $indent = sprintf "#%d e>", $self->{pid};
        print map {"$indent $_\n"} @errors;
    }

    return @response;
}

=item exit ()

The C<exit> method attempts to stop and clean up after an IOC that is still
running. It sends an C<exit> command to the IOC shell (without waiting for a
response), then calls the C<close> method to finish the task of shutting down
the IOC process and tidying up after it.

=cut

sub exit {
    my $self = $_[0];

    return ()
        unless $self->started;

    $self->_send("exit\n"); # Don't wait

    goto &close;
}

=item close ()

The C<close> method first closes the IOC's stdin stream, which will trigger an
end-of-file to the IOC shell, then it fetches any remaining lines from the
IOC's stdout stream before closing both that and the stderr stream. Finally
(unless we're running on MS-Windows) it sends a SIGTERM signal to the child
process and waits for it to clean up. A list containing the final output from
the IOC's stdout stream is returned.

=cut

sub close {
    my $self = shift;

    return ()
        unless $self->started;

    my $pid = $self->{pid};
    my $debug = $self->{debug};

    printf "#%d << <EOF>\n", $pid if $debug;
    close $self->{stdin};

    my @response = $self->_getlines; # No terminator
    close $self->{stdout};

    $self->{select}->remove($self->{stderr});
    close $self->{stderr};

    # Reset these before we call waitpid in case of timeout
    $self->{pid} = undef;
    $self->{stdin} = gensym;
    $self->{stdout} = gensym;
    $self->{stderr} = gensym;

    if ($^O ne 'MSWin32') {
        printf "#%d killing ... ", $pid if $debug;
        kill 'TERM', $pid;
        waitpid $pid, 0;
        printf "%d dead.\n", $pid if $debug;
    }

    return @response;
}

=item DESTROY ()

C<EPICS::IOC> objects have a destructor which calls the C<exit> method, but it
is not recommended that this be relied on to terminate an IOC process. Better
to use an C<END {}> block and/or trap the necessary signals and explicitly
C<exit> or C<close> the IOC.

=cut

sub DESTROY {
    shift->exit;
}


=back

=head1 CONVENIENCE METHODS

The following methods provide an easy way to perform various common IOC
operations.

=over 4

=item dbLoadRecords ( FILE [, MACROS] )

Instructs the IOC to load a database (.db) from FILE. If provided, the MACROS
parameter is a single string containing one or more comma-separated assignment
statements like C<a=1> for macros that are used in the database file.

This method can also be used to load a database definition (.dbd) file.

=cut

sub dbLoadRecords {
    my ($self, $file, $macros) = @_;

    $macros = '' unless defined $macros;
    $self->cmd('dbLoadRecords', $file, $macros);
}

=item iocInit ()

Start the IOC executing.

=cut

sub iocInit {
    shift->cmd('iocInit');
}

=item dbl ( [RECORDTYPE])

This method uses the C<dbl> command to fetch a list of all of the record names
the IOC has loaded. If a RECORDTYPE name is given, the list will only comprise
records of that type.

=cut

sub dbl {
    my ($self, $rtyp) = @_;

    return $self->cmd('dbl', $rtyp)
}

=item dbgf ( PV )

The C<dbgf> method returns the value of the process variable PV, or C<undef> if
the PV doesn't exist. This only works when the PV holds a scalar or an array
with one element.

=cut

# Regexps for the output from dbgf, currently supporting scalars only
my $RXdbfstr = qr/ DB[FR]_(STRING) : \s* " ( (?> \\. | [^"\\] )* ) " /x;
my $RXdbfint = qr/ DB[FR]_(U?(?:CHAR|SHORT|LONG|INT64)) : \s* ( -? [0-9]+ ) /x;
my $RXdbfflt = qr/ DB[FR]_(FLOAT|DOUBLE) : \s* ( [0-9.eE+\-]+ ) /x;
my $RXdbf = qr/ (?| $RXdbfstr | $RXdbfint | $RXdbfflt ) /x;

sub dbgf {
    my ($self, $pv) = @_;

    my @res = $self->cmd('dbgf', $pv);
    return undef unless scalar @res;

    my ($type, $result) = ($res[0] =~ m/^ $RXdbf /x);
    $result =~ s/\\([\\"'])/$1/gx
        if $type eq 'STRING';
    return $result;
}

=item dbpf ( PV, VALUE )

This method sets PV to VALUE, and returns the new value, or C<undef> if the PV
doesn't exist. If the put fails the return value is the previous value of the
PV. As with the C<dbgf> method this only works for scalar or single-element
arrays, but PV may be an array field which will be set to one element.

=cut

sub dbpf {
    my ($self, $pv, $val) = @_;

    my @res = $self->cmd('dbpf', $pv, $val);
    return undef unless scalar @res;

    my ($type, $result) = ($res[0] =~ m/^ $RXdbf /x);
    $result =~ s/\\([\\"'])/$1/gx
        if $type eq 'STRING';
    return $result;
}

=back

=head1 COPYRIGHT AND LICENSE

Portions Copyright (C) 2011 UChicago Argonne LLC, as Operator of Argonne
National Laboratory.

This software is distributed under the terms of the EPICS Open License.

=cut

1;

__END__
