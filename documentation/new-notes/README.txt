documentation/new-notes/README.txt
==================================

The documentation/new-notes directory is for new release note entries that
describe changes merged into EPICS Base since the previous release.
Files here must be written in Markdown (see below) and have the extension
'.md' at the end of their filename.
This README.txt file is the only other file that should appear here.

Do not add new notes by editing the RELEASE-<version>.md files in the Base
documentation directory or the O.Common/RELEASE_NOTES.md file that running
'make' generates.

Writing a Release Notes entry
-----------------------------

Add a new file to the new-notes directory for your entry. If this is for a
GitHub pull request to the epics-base project please use the name 'PR-nnn.md'
where nnn is the number of the pull request. If you haven't created the pull
request yet you can use the number from a related GitHub issue, or use some
other name, then rename and push it after the PR has been created.

The file *must* start with a level-3 Markdown title for the entry, like this:

    ### Conflict-free release note entries for GitHub pull requests

    GitHub [PR #628](https://github.com/epics-base/epics-base/pull/628)

 * The three '#' characters of the title start in the left-most column.
 * The title should provide a short summary, and not end in a period.
 * A link to the GitHub pull-request may follow if desired as shown above
   (followed by a blank line to separate it from the next paragraph), or a
   link to the PR may be integrated into the text that follows.
 * Use blank lines between paragraphs of text, and code-blocks for examples.
 * I recommend using [semantic line-breaks](https://sembr.org/) in Markdown
   files, it makes editing easier and reduces the number of lines that change
   in most commits. This README.txt file isn't formatted as Markdown.

Release note entries are not intended to provide full documentation of major
features. For small features or changes though, they may provide all the
information needed to understand and use the feature.

Generating RELEASE_NOTES.md
---------------------------

Running 'make' inside the Base documentation directory now generates a
file RELEASE_NOTES.md and installs it into the top-level doc directory.

The file starts with a level-1 Markdown header and some introductory text.
If any new-notes files are present a level-2 header is added with a Release
version number and a -DEV suffix, followed by some notes explaining their
unreleased status. The new-notes filenames are lexically sorted and their
contents added in order, separated by an extra newline character.

Finally a series of myst Markdown directives are added which will include all
the older RELEASE-<version>.md files present in the documentation directory
into the published HTML version, sorted in version order with the newest first.
