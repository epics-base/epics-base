# Contribution Guidelines

For bug fixes, please create new pull requests against the default (7.0)
branch. If the bug only affects a specific older release branch, target that
branch instead.

For new features, please open an issue or discuss the idea on the tech-talk
mailing list or [in our Matrix: EPICS General channel](https://matrix.to/#/!GpKBGNPEfGCodzzfoN:epics-controls.org?via=epics-controls.org&via=matrix.org&via=trinity.fhi.mpg.de)
beforehand.

## Use of AI

The use of LLMs such as Claude Code, Copilot, ChatGPT or Google Jules to
help find issues and write code is acceptable, but the human who directs
the AI will be held resposible for everything submitted and must understand
and review the problem and the code changes being suggested. If you are not
responsive to questions and change requests from the code reviewers your
contribution could be rejected.

Code changes developed with AI assistance must first be submitted as a
GitHub Issue, describing the problem discovered or describing new features
being proposed. Review comments may have suggestions for alternative
approaches to implementing the fix or feature, and may ask for a detailed
design to be added to the issue comments and reviewed before coding starts.

EPICS Base provides an `AI.md` file with context information for LLM tools.
Where possible we have included soft-links to this from the locations used
by the main LLM tools we know about (`CLAUDE.md`, `AGENTS.md`,
`.github/copilot-instructions.md`). Ensure that your LLM tool has read this
document and will follow it appropriately.

## Maintaining Portability

Where possible, build and test your changes on as many systems you can access
that can build and run EPICS.
The important operating systems are Linux, Windows, macOS and RTEMS.
The EPICS CI will attempt to build all Pull Requests on multiple variants of
these platforms and will run the test code where it can.

## Robustness of long-lived processes

Code that runs as part of the IOC must detect and gracefully handle resource
exhaustion (such as the OS running out of memory or being unable to create a
new socket) without crashing or stopping the IOC after iocInit has completed.

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
