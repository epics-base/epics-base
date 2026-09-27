TOP=..
include $(TOP)/configure/CONFIG

# Install the makeSiteApp script
SCRIPTS_HOST += makeSiteApp
EXPAND += makeSiteApp
EXPAND_ME += EPICS_BASE
EXPAND_VARS += MYTOP=$(abspath $(TOP))

# Standard EPICS build files
TEMPLATES_DIR = makeBaseApp

TEMPLATES += top/Makefile
TEMPLATES += top/.gitignore
TEMPLATES += top/configure/CONFIG
TEMPLATES += top/configure/CONFIG_SITE
TEMPLATES += top/configure/Makefile
TEMPLATES += top/configure/RELEASE
TEMPLATES += top/configure/RULES
TEMPLATES += top/configure/RULES.ioc
TEMPLATES += top/configure/RULES_DIRS
TEMPLATES += top/configure/RULES_TOP

# Add your template files here like this:
# TEMPLATES += top/xxxApp/Makefile
# TEMPLATES += top/xxxApp/...


include $(TOP)/configure/RULES

