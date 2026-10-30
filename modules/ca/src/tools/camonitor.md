# camonitor

``` {program} camonitor
```

    camonitor [options] <PV name> ...

## Description

Subscribe to and print value updates for PV(s).

## Options

::: {option} -h
Print usage information
:::

### CA options

::: {option} -w <sec>
Wait time, specifies longer CA timeout, default is 1.0 second.
:::

::: {option} -m <msk>
Specify CA event mask to use.

\<msk\> is any combination of `v` (value), `a` (alarm), `l` (log/archive),
`p` (property).

Default event mask is `va`
:::

::: {option} -p <prio>
CA priority (0--99, default 0=lowest)
:::

### Timestamps

Default: Print absolute timestamps (as reported by CA server)

::: {option} -t <key>
Specify timestamp source(s) and type, with \<key\> containing:

`s` = CA server (remote) timestamps\
`c` = CA client (local) timestamps (shown in `()`s)\
`n` = no timestamps\
`r` = relative timestamps (time elapsed since start of program)\
`i` = incremental timestamps (time elapsed since last update)\
`I` = incremental timestamps (time since last update, by channel)\

`r`, `i` or `I` require `s` or `c` to select the time source.
:::

### Enum format

::: {option} -n
Print `DBF_ENUM` values as number (default is enum string)
:::

### Arrays

Array values: Print number of elements, then list of values.

By default: request and print all elements (dynamic arrays supported).

::: {option} -# <count>
Request and print first \<count\> elements of an array.
:::

::: {option} -S
Print array of char as a string (long string).
:::

### Floating point format

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


