# caput

``` {program} caput
```

    caput [options] <PV name> <value> ...
    caput -a [options] <PV name> <no of elements> <value> ...

## Description

Put value to a PV.

The specified value is written to the PV (as a string). The PV's value is read
before and after the write operation and printed as "Old" and "New" values on
stdout.

There are two variants to the arguments for this command. For the scalar
variant without the {option}`-a` flag, all the value arguments provided after
the PV name are concatenated with a single space character between them, and
the resulting string (up to 40 characters long unless the {option}`-S` flag is
given) is written to the specified PV.

The array variant with the {option}`-a` flag writes an array of string values
to the specified PV. The numeric argument giving the number of array elements
is actually ignored, the array length to be written is actually controlled by
the number of values provided on the command line.

## Options

::: {option} -h
Print usage information
:::

### CA options

::: {option} -w <sec>
Wait time, specifies longer CA timeout, default is 1.0 second.
:::

::: {option} -c
Asynchronous get (use ca_get_callback instead of ca_get).
:::

::: {option} -p <prio>
CA priority (0--99, default 0=lowest)
:::

### Format options

::: {option} -t
Terse mode --- print only successfully written value, without name.
:::

::: {option} -l
Long mode `name timestamp value stat sevr` (read PVs as `DBR_TIME_xxx`)
:::

### Enum Format

By default: Auto --- try value as ENUM string, then as index number

::: {option} -n
Force interpretation of values as numbers
:::

::: {option} -s
Force interpretation of values as strings
:::

### Arrays

By default: put scalar

Value format: all value arguments concatenated with spaces

::: {option} -S
Put string as an array of chars (long string)
:::

::: {option} -a
Put array

Value format: number of values, then list of values
:::

