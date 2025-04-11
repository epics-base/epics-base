### Conflict-free release note entries for GitHub pull requests

This release replaces the single `documentation/RELEASE_NOTES.md` file in the
EPICS source tree with a mechanism that should prevent merge conflicts when
new entries are added from multiple pull requests.

In this new approach each pull request adds a separate Markdown file to the
`documentation/new-notes` directory with a unique name. All these files get
combined into a single `RELEASE-<version>.md` file when a release is made and
the `new-notes` directory is emptied ready for further development.

Instructions on creating new entries are provided in a `README.txt` file in
the `documentation/new-notes` directory. Running `make` in the `documentation`
directory will combine all the new entries into a `RELEASE_NOTES.md` file that
then gets installed into the `doc` top-level directory. The `make` command
will also install the older `RELEASE-<version>.md` files into that `doc`
directory.
