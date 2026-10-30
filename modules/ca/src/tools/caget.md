# caget

``` {program} caget
```

    caget [options] <PV name> ...

## Description

Get and print value for PV(s).

The values for one or multiple PVs are read and printed to stdout. The
`DBR_...` format in which the data is read, the output format, and a
number of details of how integer and float values are represented can be
controlled using command line options.

When getting multiple PVs, their order on the command line is retained
in the output.

## Options

::: {option} -h
Print usage information
:::

### Channel access options

::: {option} -w <sec>
Wait time, specifies longer CA timeout, default is 1.0 second.
:::

::: {option} -c
Asynchronous get (use ca_get_callback instead of ca_get).
:::

::: {option} -p <prio>
CA priority (0--99, default 0=lowest)
:::

### Format and data type options

Default output format is `name value`

::: {option} -t
Terse mode --- print only value, without name.
:::

::: {option} -a
Wide mode `name timestamp value stat sevr` (read PVs as `DBR_TIME_xxx`)
:::

::: {option} -d <type>
Request specific dbr type; use   string (`DBR_` prefix may be omitted)
or number of one of the following types:

:STRING: 0
:INT: 1
:SHORT: 1
:FLOAT: 2
:ENUM: 3
:CHAR: 4
:LONG: 5
:DOUBLE: 6
:STS_STRING: 7
:STS_INT: 8
:STS_SHORT: 8
:STS_FLOAT: 9
:STS_ENUM: 10
:STS_CHAR: 11
:STS_LONG: 12
:STS_DOUBLE: 13
:TIME_STRING: 14
:TIME_INT: 15
:TIME_SHORT: 15
:TIME_FLOAT: 16
:TIME_ENUM: 17
:TIME_CHAR: 18
:TIME_LONG: 19
:TIME_DOUBLE: 20
:GR_STRING: 21
:GR_INT: 22
:GR_SHORT: 22
:GR_FLOAT: 23
:GR_ENUM: 24
:GR_CHAR: 25
:GR_LONG: 26
:GR_DOUBLE: 27
:CTRL_STRING: 28
:CTRL_INT: 29
:CTRL_SHORT: 29
:CTRL_FLOAT: 30
:CTRL_ENUM: 31
:CTRL_CHAR: 32
:CTRL_LONG: 33
:CTRL_DOUBLE: 34
:STSACK_STRING: 37
:CLASS_NAME: 38
:::

### Enum format

::: {option} -n
Print `DBF_ENUM` value as number (default is enum string)
:::

### Arrays

Value format: Print number of requested values, then list of values.

By default: print all values.

::: {option} -# <count>
Print first \<count\> elements of an array.
:::

::: {option} -S
Print array of char as a string (long string).
:::

### Floating point type format

By default: use `%g` format.

::: {option} -e <nr>
Use `%e` format, with a precision of \<nr\> digits.
:::

::: {option} -f <nr>
Use `%f` format, with a precision of \<nr\> digits.
:::

::: {option} -g <nr>
Use `%g` format, with a precision of \<nr\> digits.
:::

::: {option} -s
Get value as string (honors server-side precision).
:::

::: {option} -lx
Round to long integer and print as hex number.
:::

::: {option} -lo
Round to long integer and print as octal number.
:::

::: {option} -lb
Round to long integer and print as binary number.
:::

### Integer number format

By default: print as decimal number.

::: {option} -0x
Print as hex number.
:::

::: {option} -0o
Print as octal number.
:::

::: {option} -0b
Print as binary number.
:::

### Alternate output field separator

::: {option} -F <ofs>
Use \<ofs\> as an alternate output field separator.
:::
