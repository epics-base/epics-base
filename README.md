
# EPICS Base - the central core of a control system toolkit

EPICS is a set of software tools and applications which provide a software infrastructure for use in building distributed control systems to operate devices such as Particle Accelerators, Large Experiments and major Telescopes. Such distributed control systems typically comprise tens or even hundreds of computers, networked together to allow communication between them and to provide control and feedback of the various parts of the device from a central control room, or even remotely over the internet. More details can be found at the official website <https://epics-controls.org/about-epics/>

Documentation: <https://docs.epics-controls.org/en/latest/>
Official Website: <https://epics-controls.org/>
Repository: <https://github.com/epics-base/epics-base>
Mailing List: <https://epics.anl.gov/>
Matrix Rooms: <https://matrix.to/#/#epics:epics-controls.org>

## Quick Install

```bash
make
```

For more information on how to install on your system see <https://docs.epics-controls.org/en/latest/getting-started/installation.html>

### Quick run a softIOC

After building, you can run an example soft-IOC (Input/Output Controller)
which uses the pvAccess network protocol.

```bash
softIocPVA
```
