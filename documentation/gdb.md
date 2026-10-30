# Debugging with GDB

Starts a process in [GDB](https://sourceware.org/gdb/current/onlinedocs/gdb.html/):

```sh
gdb --args ../../bin/linux-x86_64-debug/myioc ./st.cmd
```

Attach to a running process by numeric PID:

```sh
gdb --pid=##
```

Shorthand if only one instance is running:

```sh
gdb --pid=`pgrep myioc`
```

## GDB shell Cheat sheet

Print stack traces of all threads.  Optionally with pagination disabled:

```gdb
(gdb) set pagination off

(gdb) thread apply all backtrace

(gdb) set pagination on
```

Note: consider sending or posting this very long output as an attachment file.

Break when a certain record begins processing:

```gdb
(gdb) break dbProcess if $_streq(precord.name, "my:record")
```

Break when a `PACT` field changes:

```gdb
(gdb) watch -l $record("my:record").pact
```

Note: Uses custom `$record()` helper.  See below.

## Helpers

If the bundled `epics-gdb-helper.py` is [enabled](#helper-enable),
then a set of pretty printers are registered for some common database types.
eg. `dbCommon*` will show the record name.
`DBLINK*` will show the associated record name, and the link target.
Also, several convenience are available.


Use `$record()` to lookup a record by name:

```gdb
(gdb) print $record("my:record")
$1 = (calcRecord*)(0x55555562f6f8, "my:record")
```

Note: `$record()` may only be called after record allocation during `iocInit()`.

Use `$rcast()` to up-cast `dbCommon*` to the specific record type:

```gdb
(gdb) print precord
$1 = (dbCommon*)(0x55555562f6f8, "my:record")
(gdb) print $rcast(precord)
$2 = (calcRecord*)(0x55555562f6f8, "my:record")
```

Use `$elln()` to index into `ELLLIST` linked lists.

```gdb
(gdb) print $elln(pdbbase.recordTypeList, 2)
```

### Enable (helper-enable)


To automatically [enable](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Auto_002dloading-safe-path.html),
add a line to `~/.gdbinit`.

```
add-auto-load-safe-path /path/to/base/checkout/
```

Or each time run:

```
(gdb) source /path/to/base/checkout/modules/database/src/ioc/epics-gdb-helper.py
```

Note: Entries in the `safe-path` list may be files (`.so` or `.py`), or directories.
      When a directory is included, sub-directories are also trusted.
      eg. `add-auto-load-safe-path /opt/epics` would also trust a `/opt/epics/lib`.

Afterwards verify at GDB shell by running:

```gdb
(gdb) info auto-load python-scripts
Loaded  Script
...
Yes     epics-gdb-helper.py

(gdb) info pretty-printer
global pretty-printers:
  EPICS
...
```
