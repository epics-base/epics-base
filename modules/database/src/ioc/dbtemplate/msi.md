# msi: Macro Substitution and Include Tool

(msitool)=
## Introduction

msi is a general purpose macro substitution/include tool.
It accepts as input an ascii template file. It looks for lines containing two reserved
command names: `include` and `substitute`. It also looks for and performs
substitutions on macros of the form `$(var)` and `${var}`. It uses the
macLib routines from EPICS Base to perform the substitutions, so it also
accepts the default value and value definition syntax that macLib
implements.

msi also allows substitutions to be specified via a separate
substitution file. This substitution file allows the same format as the
substitution files accepted by the EPICS IOC's dbLoadTemplate command.


## Command Syntax

`msi -V -g -o outfile -I dir -M subs -S subfile template`

All parameters are optional. The -o, -I, -M, and -S switches may be
separated from their associated value string by spaces if desired.
Output will be written to stdout unless the -o option is given.

Switches have the following meanings:

- **-V**

    Verbose warnings; if this parameter is specified then any undefined
    macro discovered in the template file which does not have an
    associated default value is considered an error. An error message is
    generated, and when msi terminates it will do so with an exit status
    of 2.
- **-g**

    When this flag is given all macros defined in a substitution file
    will have global scope and thus their values will persist until a
    new value is given for this macro. This flag is provided for
    backwards compatibility as this was the behavior of previous
    versions of msi, but it does not follow common scoping rules and is
    discouraged.
- **-o _file_**

    Output will be written to the specifed file rather than to the
    standard output.
- **-I _dir_**

    This parameter, which may be repeated or contain a colon-separated
    (or semi-colon separated on Windows) list of directory paths,
    specifies a search path for include commands. For example:

        msi -I /home/mrk/examples:. -I.. template

    specifies that all named files should be searched for in the following locations, 
    in the order given:

     1. /home/mrk/examples
     2. . (the current directory)
     3. .. (the parent of the current directory)

    Note that relative path searching is handled as
    
        $ cat foo.substitutions
        file rel/path/bar.template {
          # contents
        }
        $ msi -I . -I /some/path foo.substitutions
    
    which will try to find `bar.template` at the path `./rel/path/` followed by
    `/some/path/rel/path`.


- **-M _substitutions_**

    This parameter specifies macro values for the template instance.
    Multiple macro values can be specified in one substitution
    parameter, or in multiple -M parameters. For example:

        msi -M "a=aval,b=bval" -Mc=cval template

    specifies that in the template file each occurrence of:

    - `$(a)` or `${a}` is replaced by _aval_
    - `$(b)` or `${b}` is replaced by _bval_
    - `$(c)` or `${c}` is replaced by _cval_

- **-S _subfile_**

    The substitution file. See below for format.
- **_template_**

    The input file. If no file is specified then input is taken from
    stdin, i.e. msi can be used as a filter. See below for a description
    of commands that can be embedded in the template file.

It is not possible to display usage by just typing msi since executing
the command with no arguments is a valid command. To show usage specify
an illegal switch, e.g.

    msi -help


## Exit Status

- **0**
    Success.
- **1**
    Can't open/create file, or other I/O error.
- **2**
    Undefined macros encountered with the -V option specified.


## Template File Format

This file contains the text to be read and written to the output after
macro substitution is performed. If no file is given then input is read
from stdin. Variable instances to be substituted by macro values are
expressed in the template using the syntax \$(_name_) or \${_name_}. The
template can also provide default values to be used when a macro has not
been given a value, using the syntax \$(_name_=_default_) or \${_name_=_default_}.

For example, using the command

    msi -M name=Marty template

where the file template contains

    My name is $(name)
    My age is $(age=none of your business)

results in this output:

    My name is Marty
    My age is none of your business

Macro variables and their default values can be expressed in terms of
other macros if necessary, to almost any level of complexity. Recursive
definitions will generate warning messages on stderr and result in
undefined output.

The template file is read and processed one line at a time, where the
maximum length of a line before and/or after macro expansion is 1023
characters; longer input or output lines will cause msi to fail. Within
the context of a single line, macro expansion does not occur when the
variable instance appears inside a single-quoted string, or where the
dollar sign $ is preceded by a back-slash character \, but as with the
standard Unix shells, variables inside double quoted strings are
expanded properly.

However neither back-slash characters nor quotes of either variety are
removed when generating the output file, so depending on what is being
output the single quote behaviour may not be useful and may even be a
hinderance. It cannot be disabled in the current version of msi.


### Template file commands

In addition to the regular text and variable instances described above,
the template file may also contain commands which allow the insertion of
other template files and the ability to set macro values inside the
template file itself. These commands are:

    include "file"
    substitute "var=value,var=value,..."

Lines containing commands must be in one of these forms:

  - include "_filename_"

  - substitute "_name1=value1, name2=value2, ..._"

White space is allowed before and after the command verb, and after the
quoted string. If embedded quotes are needed, the backslash character \
can be used as an escape character. For example

    substitute "a=\"val\""

specifies that (unless a is subsequently redefined) wherever a $(a)
macro appears in the template below this point, the text
"val" (including the double quote characters) will appear in the output
instead.

If a line does match either syntax above it is just passed to macLib for
processing without any notification. Thus the input line:

    include "myfile" #include file

would just be passed to macLib, i.e. it would _not_ be considered an
include command.

As an example of these commands, let the Unix command be:

    msi template

and file includeFile contain:

    first name is ${first}
    family name is ${family}

and template is

    substitute "first=Marty,family=Kraimer"
    include "includeFile"
    substitute "first=Irma,family=Kraimer"
    include "includeFile"

then the following is written to the output.

    first name is Marty
    family name is Kraimer
    first name is Irma
    family name is Kraimer

Note that the IOC's dbLoadTemplate command does not support the
substitute syntax in template files, although the include syntax is
supported.


## Substitution File Format

The optional substitution file has three formats: regular, pattern, and
dbTemplate format. We will discuss each separately.


### Regular format

    global {gbl_var1=gbl_val1, gbl_var2=gbl_val2, ...}
    {var1=set1_val1, var2=set1_val2, ...}
    {var2=set2_val2, var1=set2_val1, ...}
    global {gbl_var1=gbl_val3, gbl_var2=gbl_val4, ...}
    {var1=set3_val1, var2=set3_val2, ...}
    {var2=set4_val2, var1=set4_val1, ...}

The template file is output with macro substitutions performed once for
each set of braces containing macro replacement values.


### Pattern format

    global {gbl_var1=gbl_val1, gbl_var2=gbl_val2, ...}
    pattern {var1, var2, ...}
    {set1_val1, set1_val2, ...}
    {set2_val1, set2_val2, ...}
    pattern {var2, var1, ...}
    global {gbl_var1=gbl_val3, gbl_var2=gbl_val4, ...}
    {set3_val2, set3_val1, ...}
    {set4_val2, set4_val2, ...}

This produces the same result as the regular format example above.


### dbLoadTemplate Format

This format is an extension of the format accepted by the EPICS IOC
command dbLoadTemplate, and allows templates to be expanded on the host
rather by using dbLoadTemplate at IOC boot time.

    global {gbl_var1=gbl_val1, gbl_var2=gbl_val2, ...}
    file templatefile {
        /pattern format or regular format/
    }
    file "${WHERE}/template2" {
        /pattern format or regular format/
    }

For the dbTemplate format, the template filename does not have to be
given on the command line, and is usually specified in the substitutions
file instead. If a template filename is given on the command line it
will override the filenames listed in the substitutions files.


### Syntax for all formats

A comment line may appear anywhere in a substitution file, and will be
ignored. A comment line is any line beginning with the character #,
which must be the very first character on the line.

Global definitions may supplement or override the macro values supplied
on the command-line using the -M switch, and set default values that
will survive for the remainder of the file unless another global
definition of the same macro changes it.

For definitions within braces given in any of the file formats, a
separator must be given between items. A separator is either a comma, or
one or more of the standard white space characters (space, formfeed,
newline, carriage return, tab or vertical tab).

Each item within braces can be an alphanumeric token, or a double-quoted
string. A back-slash character \ can be used to escape a quote character
needed inside a quoted string. These three sets of substitutions are all
equivalent:

    {a=aa b=bb c="\"cc\""}
    {b="bb",a=aa,c="\"cc\""}
    {
        c="\"cc\""
        b=bb
        a="aa"
    }

Within a substitutions file, the file name may appear inside double
quotation marks; these are required if the name contains certain
characters or environment variable macros of the form `${ENV_VAR}` or
`$(ENV_VAR)`, which will be expanded before the file is opened.


### Regular substitution example

Let the command be:

    msi -S substitute template

The file template contains

    first name is ${first}
    family name is ${family}

and the file `substitute` is

    global {family=Kraimer}
    {first=Marty}
    {first=Irma}

The following is the output produced:

    first name is Marty
    family name is Kraimer
    first name is Irma
    family name is Kraimer


### Pattern substitution example

Let the command be:

    msi -S pattern template

The file pattern contains

    pattern {first,last}
    {Marty,Kraimer}
    {Irma,Kraimer}

and template is the same as the previous example:

    first name is ${first}
    family name is ${family}

This is the output:

    first name is Marty
    family name is Kraimer
    first name is Irma
    family name is Kraimer


### dbTemplate example

Let the command be

    msi -S xxx.substitutions

`xxx.substitutions` is

    file template {
    pattern {first,last}
    {Marty,Kraimer}
    {Irma,Kraimer}
    pattern {last,first}
    {Smith,Bill}
    {Smith,Mary}
    }
    file template {
    {first=Marty,last=Kraimer}
    {first=Irma,last=Kraimer}
    }

`template` is the same as in the previous example.

The following is written to the output

    first name is Marty
    family name is Kraimer
    first name is Irma
    family name is Kraimer
    first name is Bill
    last name is Smith
    first name is Mary
    last name is Smith
    first name is Marty
    family name is Kraimer
    first name is Irma
    family name is Kraimer

