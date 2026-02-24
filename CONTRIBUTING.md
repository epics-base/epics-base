# Contribution Guidelines

For bug fixes, please create new pull requests against the default (7.0)
branch. If the bug only affects a specific older release branch, target that
branch instead.

For new features, please open an issue or discuss the idea on mailing lists
or in Matrix beforehand.

## Maintaining Compatibility

Build and test your changes on as many systems as possible.
Important operating systems are Linux, Windows, macOS, vxWorks and RTEMS.

## Testing

All new features must come with automated tests to verify their correctness.
This also helps to find out if future changes break existing features.

EPICS Base comes with a testing framework which allows you to run IOCs,
set and read/compare values and more.

See any of the existing tests within the repository for guidance on test
structure and dependencies.

Add your tests to the appropriate test directory and ensure they are
included in the test Makefile.

Your test should run (and succeed) when you execute:

```bash
make runtests
```

CI will run these tests automatically. All checks must pass before a
pull request can be merged.

## Review Process

The core developer team will review your changes, suggest changes, highlight
issues or merge your code into EPICS Base.
