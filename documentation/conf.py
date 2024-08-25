# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# http://www.sphinx-doc.org/en/master/config

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
import os
import sys

sys.path.insert(0, os.path.abspath("."))

# -- Project information -----------------------------------------------------

project = "EPICS Base Documentation"
copyright = "EPICS Controls"
author = "The EPICS Collaboration"

# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.

extensions = [
    "hoverxref.extension",
    "breathe",
    "sphinx.ext.mathjax",
    "sphinx.ext.ifconfig",
    "sphinx.ext.graphviz",
    "sphinx_copybutton",
    "sphinx.ext.intersphinx",
    'myst_parser',
]

# Setup the breathe extension
breathe_projects = {"epics-base": "xml"}

breathe_default_project = "epics-base"

# Tell sphinx what the primary language being documented is.
primary_domain = "cpp"

# Tell sphinx what the pygments highlight language should be.
highlight_language = "cpp"

# Add any paths that contain templates here, relative to this directory.
# templates_path = ()

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store", "O.*", "venv"]

# Intersphinx links to subprojects
intersphinx_mapping = {
    "epics": ("https://docs.epics-controls.org/en/latest/", None),
}
intersphinx_disabled_reftypes = ["*"]
hoverxref_role_types = {
    "hoverxref": "tooltip",
    "ref": "modal",
    "confval": "tooltip",
    "mod": "modal",
    "class": "modal",
    "obj": "tooltip",
}

hoverxref_intersphinx_types = {
    "readthedocs": "modal",
    "sphinx": "tooltip",
}

hoverxref_domains = [
    "py",
]

# Enabled Markdown extensions.
# See here for what they do:
# https://myst-parser.readthedocs.io/en/latest/syntax/optional.html
myst_enable_extensions = [
    "amsmath",
    "colon_fence",
    "deflist",
    "dollarmath",
    "fieldlist",
    "html_image",
    "replacements",
    "smartquotes",
    "strikethrough",
    "tasklist",
]

# Allows auto-generated header anchors:
# https://myst-parser.readthedocs.io/en/latest/syntax/optional.html#auto-generated-header-anchors
myst_heading_anchors = 4


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = "sphinx_rtd_theme"


# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
# html_static_path = ['_static']

# html_css_files = [
#    'css/custom.css',
# ]

master_doc = "index"

# html_theme_options = {
#    'logo_only': True,
# }
# html_logo = "images/EPICS_white_logo_v02.png"

# html_extra_path = ['../html']

# Breathe directives
