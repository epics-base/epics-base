# EPICS Base Release Procedures & Checklist

This document describes the procedures and provides a checklist of tasks
that should be performed when creating production releases of EPICS
Base.

## The Release Process

The decision to make a new release is taken during the
Core Developers bi-weekly meetings in an informal manner. The steps
detailed below were written to remind Andrew (or whoever does
the release) exactly what has to be done, since it's so easy to
miss steps.

### Roles

The following roles are used below:

**Release Manager**
Responsible for managing and tagging the release

**Core Developers**
Responsible for maintaining the EPICS software

**Website Editors**
Responsible for the EPICS websites

----

### Preparing for a release

**Release Manager:**
Notify core developers about the upcoming release and ask about any
remaining tasks that must be finished.

**All developers:**
Check the bug tracker for any outstanding items and handle
appropriately.

**Release Manager:**
Set a Feature Freeze date, by which time all Git branches for
enhancements and new functionality should have been merged. After this
date, commits and merges should only be made to fix problems that show
up during testing.

**Release Manager & all developers:**
Request that documentation be updated and information about new
features be added before the release date:

- Release Notes
- Doxygen annotations
- Other documents on
  [docs.epics-controls.org](https://docs.epics-controls.org/en/latest/)

**Release Manager:**
Review and update this checklist for the upcoming release. Update
the release version number in the tags and messages below.

----

### Testing

**All Developers:**
Run the internal test programs on supported platforms that aren't checked by CI.

**ALL Developers:**
Check that all makeBaseApp templates build and run properly,
all _xxxApp_ and _xxxBoot_ types and any internal options,
e.g. setting `STATIC_BUILD=YES` or using a different
`INSTALL_LOCATION` in `configure/CONFIG_SITE`.

**Release Manager:**
Check that documentation has been updated:

- Release Notes
- Doxygen annotations
- Other documents on
  [docs.epics-controls.org](https://docs.epics-controls.org/en/latest/)

**Core Developers:**
Reach a consensus that the software is ready to release.

----

### Creating the final release version

#### A. For each external submodule to be tagged

**Release Manager:**

1. `cd base-7.0/modules/<module>; git grep UNRELEASED`
   and insert the submodule's version number into any doxygen annotations
   that have a `@since UNRELEASED` comment. Commit (don't push yet).

2. Check that the submodule's Release Notes have been updated to cover
   all changes; add missing items as necessary, and set the module version
   number and release date if appropriate.
   Commit the changes to the submodule's Notes file (don't push).

3. Copy the new submodule version number and Release Notes entries into
   a new file named `<module>-<release>.md`
   in the `base-7.0/documentation/new-notes` directory.

4. Edit the module's release version file
   `configure/CONFIG_<module>_VERSION`
   and any `Doxyfile` in the top-level and/or `documentation`
   directories. In these, set `DEVELOPMENT_FLAG=0` and remove
   `-dev` from the `PROJECT_NUMBER` string. Commit
   these changes (don't push):

   ```
   git commit -m "Final commit for <submodule-version>"
   ```

5. Tag the submodule (replace `ANJ` with your initials):

   ```
   git tag -m "ANJ: Tag for EPICS 7.0.11" <submodule-version>
   ```

6. Generate documentation for modules with
   `release_notes.dox` files. Prepare to update the github-pages
   website as follows:

   ```
   cd base-7.0/modules/<module>/documentation
   make commit
   git push --force upstream gh-pages
   ```

   _Q: Delay this `git push` until later?_

7. Update the Git submodule on the Base-7.0 branch to the
   newly-tagged version, check the module's status matches the tag:

   ```
   cd base-7.0/modules
   git add <module>
   git submodule status --cached
   ```

   Don't commit the submodule updates yet.

8. Edit the module's release version file
   `configure/CONFIG_<module>_VERSION`
   and its top-level `Doxyfile`; increment the
   `MAINTENANCE_VERSION`, set the `DEVELOPMENT_FLAG`
   value to 1, and update the `PROJECT_NUMBER` string, appending
   `-dev` to the new module version number. Commit changes.

9. Push commits and the new tag to the submodule's GitHub repository
   (assumed to be the `upstream` remote):

   ```
   cd base-7.0/modules/<module>
   git push --follow-tags upstream master
   ```

#### B. After all submodules have been updated

**Release Manager:**

1. Commit the submodule updates which were added for each submodule
   in step 7 above to the 7.0 branch (don't push):

   ```
   cd base-7.0/modules
   git commit -m "Update git submodules for release"
   ```

2. Make sure that the output from
   `git submodule status --cached` only shows the appropriate
   version tags in the right-most parenthesized column with no
   `-<n>-g<xxxxxxx>` suffix.

3. Add and commit the new Release Note entry files that were created
   for each submodule in step 3 above (don't push):

   ```
   cd base-7.0/documentation
   git add new-notes
   git commit -m "Add submodule release note entries"
   ```

#### C. In the main epics-base repository

**Release Manager:**

1. `cd base-7.0; git grep UNRELEASED` and insert the release
   version to any doxygen annotations that have a
   `@since UNRELEASED` comment. Commit (don't push).

2. Edit the main EPICS Base version file and the built-in module
   version files:
   - `configure/CONFIG_BASE_VERSION`
   - `configure/CONFIG_LIBCOM_VERSION`
   - `configure/CONFIG_CA_VERSION`
   - `configure/CONFIG_DATABASE_VERSION`

3. Version numbers should be set according to the level of changes made
   since the last release. Note that the `MAINTENANCE_VERSION`
   or `PATCH_LEVEL` value will have been incremented immediately
   after the previous release tag was applied, so don't double-increment
   them. Set all `DEVELOPMENT_FLAG` values to 0 and set the
   `EPICS_DEV_SNAPSHOT` to an empty string (no quotes).

4. Commit the above changes (don't push):

   ```
   cd base-7.0
   git add configure/CONFIG_*_VERSION
   git commit -m "Set core version numbers for release"
   ```

5. When `EPICS_DEV_SNAPSHOT` is empty because a release
   is being created, the `documentation/Makefile` supports a
   build target `release` for creating a new release notes file
   `documentation/RELEASE-<version>.md`
   from the Markdown files in the `documentation/new-notes` directory.
   When run, it copies the notes entries from all the
   `new-notes/*.md` files, then deletes the files and prepares a
   Git commit to apply those changes permanently to the repository.

   Run these commands to generate the `RELEASE-7.0.11.md`
   file and remove the individual release note entry files:

   ```
   cd base-7.0/documentation
   make release
   ```

6. The `make release` command adds some changes into the
   Git index but doesn't commit them. These commands let you check what
   was done and commit the result (don't push yet!):

   ```
   git status
   git diff --staged
   git commit -m "Generate RELEASE-7.0.11.md notes file"
   ```

   If you need to undo those Git actions before committing and confirm
   that happened, use `make unrelease; git status`

#### D. Tag the main repository

**Release Manager:**
Tag the epics-base module in Git (replace `ANJ` with your initials):

```
cd base-7.0
git tag -m "ANJ: Tagged for release" R7.0.11
```

Don't push to GitHub yet.

#### E. After tagging

**Release Manager:**
Edit the main EPICS Base version file and the built-in module version
files:

- `configure/CONFIG_BASE_VERSION`
- `configure/CONFIG_LIBCOM_VERSION`
- `configure/CONFIG_CA_VERSION`
- `configure/CONFIG_DATABASE_VERSION`

Version numbers should be set for the next expected patch/maintenance
release by incrementing the MAINTENANCE_VERSION or PATCH_LEVEL value in
each file. Set all `DEVELOPMENT_FLAG` values to 1 and
`EPICS_DEV_SNAPSHOT` to `-DEV`.

Commit these changes (don't push).

**Release Manager:**
Export the tagged version into a tarfile. The
`make-tar.sh` script generates a gzipped tarfile directly
from the tag, excluding the files and directories that are only used for
continuous integration:

```
cd base-7.0
./.tools/make-tar.sh R7.0.11 ../base-7.0.11.tar.gz base-7.0.11/
```

Create a GPG signature file of the tarfile as follows:

```
cd ..
gpg --armor --sign --detach-sig base-7.0.11.tar.gz
```

**Release Manager:**
Test the tar file by extracting its contents and building it on at
least one supported platform. If this succeeds the commits and new git
tag can be pushed to the GitHub repository's 7.0 branch (assumed to be
the `upstream` remote):

```
git push --follow-tags upstream 7.0
```

----

### Publish the Release

These stages don't have to happen in this particular order.

#### A. Publish to epics.anl.gov

**Website Editor:**
Copy the tarfile and its signature to the Base download area of the
website.

**Website Editor:**
Add the new release tar file to the website Base download index
page.

**Website Editor:**
Create or update the website subdirectory that holds the release
documentation, and copy in the files to be published with this release
version.

**Website Editor:**
Update the webpage for the new release with links to the release
documents and tar file.

**Website Editor:**
Link to the release webpage from other relevant areas of the website,
updating the front page and sidebar.

**Website Editor:**
Add an entry to the website News page, linking to the new version
webpage.

### B. Publish to epics-controls.org

**Website Editor:**
Upload the tar file and its `.asc` signature file to the
epics-controls web-server.

```
scp base-7.0.11.tar.gz base-7.0.11.tar.gz.asc epics-controls:download/base
```

**Website Editor:**
Follow instructions on
[Add a page for a new release](https://epics-controls.org/resources-and-support/documents/epics-website-documentation/adding-a-page-for-a-new-release/)
to create a new release webpage (not required for a patch release, just
edit the existing page). Update the TablePress "Point Releases" table
and add the new download, and adjust the Html Snippet for the series
download.

Not covered in the instructions on the website: Go to Posts, find a previous
release and use "Duplicate Post", then edit the result and publish it.
This generates the News item.

### C. Publish to GitHub

**Release Manager:**
Go to the GitHub
[Create release from tag R7.0.11](https://github.com/epics-base/epics-base/releases/new?tag=R7.0.11)
page. Upload the tar file and its `.asc` signature file to the new
GitHub release page, or just drag-n-drop them into the page. Copy/paste
the text from the previous release and edit. Submit.

**Release Manager:**
We used to close out bug reports in Launchpad at release-time, this
would be the time to do that if we want an equivalent on GitHub.

----

### Make Announcement

**Release Manager:**
Announce the release on the tech-talk mailing list, and possibly on the EPICS
Matrix Chat server.
