# cainfo

``` {program} cainfo
```

    cainfo [options] <PV name> ...

## Description

Get and print channel and connection information for PV(s).

All available Channel Access related information about PV(s) is printed
to stdout.

The {option}`-s` option allows to specify an interest level for calling Channel
Access' internal report function `ca_client_status()`, that prints lots
of internal informations on stdout, including environment settings, used
CA ports etc.

## Options

::: {option} -h
Print usage information
:::

### CA options

::: {option} -w <sec>
Wait time, specifies longer CA timeout, default is 1.0 second.
:::

::: {option} -s <level>
Call ca_client_status with the specified interest level
:::

::: {option} -p <prio>
CA priority (0--99, default 0=lowest)
:::


