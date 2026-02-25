# Build epics-base's documentation

To build epics-base documentation,
you need the following dependencies:

- Python with Pip
- Doxygen
- Graphviz
- General epics-base requirements.
  See {ref}`software-requirements`.

## Setting up the Python environment

Create a new virtual environment under `documentation/.venv`,
if it doesn't already exists:

```bash
python -m venv documentation/.venv
```

Enter the Python environment:

```{eval-rst}
+-------------+------------+--------------------------------------------------+
| Platform    | Shell      | Command to activate virtual environment          |
+=============+============+==================================================+
| POSIX       | bash/zsh   | ``source documentation/.venv/bin/activate``      |
|             +------------+--------------------------------------------------+
|             | fish       | ``source documentation/.venv/bin/activate.fish`` |
|             +------------+--------------------------------------------------+
|             | csh/tcsh   | ``source documentation/.venv/bin/activate.csh``  |
|             +------------+--------------------------------------------------+
|             | pwsh       | ``documentation/.venv/bin/Activate.ps1``         |
+-------------+------------+--------------------------------------------------+
| Windows     | cmd.exe    | ``documentation/.venv\\Scripts\\activate.bat``   |
|             +------------+--------------------------------------------------+
|             | PowerShell | ``documentation/.venv\\Scripts\\Activate.ps1``   |
+-------------+------------+--------------------------------------------------+
```

Inside your Python virtual environment,
install the Python dependencies like so:

```bash
pip install ./documentation
```

:::{important}
Make sure you install Python dependencies inside a Python virtual environment,
**not** in the global environment.
:::

## Build the documentation

To build the documentation,
run:

```bash
make inc
make -C documentation sphinx
```
