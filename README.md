
# EPICS Base

EPICS (Experimental Physics and Industrial Control System) is a set of software
tools and applications which provide a software infrastructure for use in
building distributed control systems to operate devices such as Particle
Accelerators, Large Experiments and major Telescopes. EPICS Base is the central
core of the control system toolkit. More details can be found at the
[About page of the official website](https://epics-controls.org/about-epics/)

## Links

- [Official Website](https://epics-controls.org/)
- [Original Website](https://epics.anl.gov/)
- [Repository](https://github.com/epics-base/epics-base)

### Documentation

- [Documentation](https://docs.epics-controls.org/en/latest/)
- [Documentation Repository](https://github.com/epics-docs/epics-docs)

### Community Communication

- [Tech-Talk Mailing List](https://epics.anl.gov/tech-talk/)
- [Matrix Rooms](https://matrix.to/#/#epics:epics-controls.org)
- [News](https://epics-controls.org/news-and-events/)

## Quick Install

Download a release from the
[Downloads page](https://epics-controls.org/resources-and-support/base/downloads)
and unpack it. Inside the unpacked folder run:

```bash
make
```

For more information on how to install on your system see the
[Installation page](https://docs.epics-controls.org/en/latest/getting-started/installation.html)
of the documentation.

### Quick run a softIOC

After building, you can run an example soft-IOC (Input/Output Controller)
which uses the pvAccess network protocol.

```bash
./bin/*/softIoc -x first
```

You can then run `dbl` to get:

```bash
epics> dbl
first:BaseVersion
first:exit
epics> 
```

## License

EPICS Base is distributed subject to a Software License
Agreement found in the file [LICENSE](./LICENSE) that is included with
this distribution.
