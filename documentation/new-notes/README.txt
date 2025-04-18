documentation/new-notes/README.txt
==================================

The documentation/new-notes directory is for new release note entries that
describe changes merged into EPICS Base since the previous release.
Files here must be written in Markdown (see below) and have the extension
'.md' at the end of their filename.
This README.txt file is the only other file that should appear here.


Generating RELEASE_NOTES.md
---------------------------

Running 'make' inside the Base documentation directory will generate a
file RELEASE_NOTES.md and install it into the top-level doc directory.
The contents of the generated file are assembled by lexically sorting the
filenames of the other files in this new-notes directory and concatenating
the file contents in order, separated by an extra newline character.

The file gets a level-1 Markdown header and an explanation, followed by a
level-2 header giving the Release version number. If the software is still
a release snapshot, some extra lines are added explaining that before the
concatenated note entries.


Writing a Release Notes entry
-----------------------------

Add a new file to the new-notes directory for your entry. If this is for a
GitHub pull request to the epics-base project please use the name 'PR-nnn.md'
where nnn is the number of the pull request. If you haven't created the pull
request yet you can use the number from a related GitHub issue, or use some
other name, then rename and push it after the PR has been created.

The file should start with a level-3 Markdown title for the entry, like this:

    ### Conflict-free release note entries for GitHub pull requests

    GitHub [PR #628](https://github.com/epics-base/epics-base/pull/628)

 * The three '#' characters of the title start in the left-most column.
 * The title should provide a short summary, and not end in a period.
 * The link to the GitHub pull-request should follow if appropriate, as its
   own paragraph.
 * A blank line must follow, then as many paragraphs of text and code-blocks
   as are needed to describe the change, without going into too much detail.

Release note entries are not intended to provide full documentation of major
features. For small features or changes though, they may provide all the
information needed to understand and use the feature.
