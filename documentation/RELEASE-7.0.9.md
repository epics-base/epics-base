# Release Notes

This document describes the changes that were included in the release of EPICS
noted below. Release entries are now provided in a separate document for each
version in the EPICS 7 series, but all are combined into a single page for
publishing on the EPICS website. Separate release documents are also included
from the older Base 3.15 and 3.16 series.

The external PVA submodules continue to maintain their own release notes files
as before, but the entries describing changes in those submodules since version
7.0.5 have been copied into the associated EPICS Release Notes files, and will
be added to new EPICS Release Notes published in the future.

## EPICS Release 7.0.9

### Core documentation published at ReadTheDocs

The `documentation` directory's `Makefile` can now run various publication scripts including Sphinx and Doxygen to generate formatted documentation that is now being published
[at docs.epics-controls.org](https://docs.epics-controls.org/projects/base/en/latest/index.html)
and integrated into the main [EPICS Documentation website](https://docs.epics-controls.org/en/latest/index.html).
The best place to find out more about these mechanisms is the
[Contribution Guide](https://docs.epics-controls.org/en/latest/CONTRIBUTING.html)
although it doesn't currently cover the new processes added to epics-base.

Much of the documentation generated from .dbd.pod files at build time is now
also being converted into MarkDown (.md) files and installed into the top-level
`doc` directory. Some users might find it quicker to look up information about a
record type by opening these files in a text editor intead of opening a browser
and loading the HTML versions or finding and opening the files from the EPICS
Documentation site.

### Post monitors from compress record when it's reset

Writing into a compress record's `RES` field now posts a monitor event instead
of only changing `VAL`. Monitor clients will therefore receive an empty array.

### The AMSG error message propagates through MSS links

A database link with the MSS attribute will now propagate not only SEVR and
STAT, but also AMSG. This field contains additional information that complements
STAT. Links with MS or MSI attributes do not propagate STAT, and therefore do
not propagate AMSG, either.

Channel Access links do not propagate AMSG, regardless of the MSS attribute,
because the message is not available as Channel metadata.

### Reloading record aliases

Aliases can now be defined more than once as long as they still refer to the
same record, unless the global variable `dbRecordsOnceOnly` is non-zero.
This allows database files to be loaded multiple times, even if they contain
alias definitions.

### `DBE_PROPERTY` event rate changed

Updating property fields now only posts `DBE_PROPERTY` events if the
field actually changed.

### Changes to msi related to include paths

There are two changes to `msi` included here.

`msi` now treats files included by .template or .substutiions files in a more
consistent way: for relative paths, it will always look relative to the current
working directory if no `-I` flags are passed, and if they are passed then it
will search for the _relative_ path from each of those flags. That is, the
following will now find the file `bar.template` located at
`/some/path/rel/path/bar.template`
```
$ cat foo.substitutions
file rel/path/bar.template {
 # contents
}
$ msi -I /some/path foo.substitutions
```

Note that this does provide one change from previous behaviour: when opening a
file from the command line, `msi` will not use the `-I`-specified paths to
search for the file, but will only work relative to the current working
directory, consistent with most commandline utilities.

### Allow users to delete previously created records from the database

From this release, record instances and aliases that have already been loaded
by an IOC can be removed from the database again before the call to iocInit
by loading a second instance of the named records but using `"#"` in place of
the record type. Values for the fields are not required or advised, just use
an empty record body `{}`. This is useful when a template defines records that
are not wanted in some IOCs, without having to split or duplicate the original
template.

For example this will remove the record named "unwanted":

```
record("#", "unwanted") {}
```

### Only keep readline history for interactive sessions

Previously, all IOCsh commands were persisited in the libreadline history
(when readline support is included).
Going forward, only interactive commands are saved.

### Type change to asTrap serverSpecific data

Change `void*` to `dbChannel*` in `asTrapWriteBeforeWithData()` and
`asTrapWriteMessage::serverSpecific` to reflect the reality since
the `dbAddr*` to `dbChannel*` migration.
External code wishing to support both before and after 3.15 should
already be conditionally casting to/from the appropriate type.

### Fix issues with `_FORTIFY_SOURCE=3`

This release fixes the false positives failures whhen building with `_FORTIFY_SOURCE` level 3.
The override introduced in 7.0.8.1 has been removed.

### Other

- genVersionHeader: work with git submodules and worktrees.
- avoid UB with self `pthread_join()`
- freebsd: Add support for x86 and amd64 builds
- Clear AMSG when SEVR becomes zero.
- `seqRecord` fix support for link `DLY0`
- Add `ABORT_ON_ASSERT` flag to `CONFIG_SITE_ENV`
- rationalize osdMutex

### Submodule updates

The pvDatabase module was updated to version 4.7.2:

* Resolved issue with changed field set in the case where the top level (master)
field ("_") is not requested by the client, but the master field callback causes
all fields to be marked as updated, rather than only those fields that have
actually been modified.
