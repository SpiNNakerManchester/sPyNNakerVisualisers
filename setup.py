# Copyright (c) 2017-2021 The University of Manchester
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

from setuptools import setup, find_packages

__version__ = None
exec(open("spynnaker_visualisers/_version.py").read())
assert __version__

setup(
    name="sPyNNaker_visualisers",
    version=__version__,
    packages=find_packages(),

    # Metadata for PyPi
    url="https://github.com/SpiNNakerManchester/sPyNNakerVisualisers",
    description="Visualisation clients for special sPyNNaker networks.",
    license="GPLv2",
    classifiers=[
        "Development Status :: 4 - Beta",

        "Intended Audience :: Developers",
        "Intended Audience :: Science/Research",

        "License :: OSI Approved :: GNU General Public License v2 (GPLv2)",

        "Operating System :: POSIX :: Linux",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: MacOS",

        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
    ],
    keywords="spinnaker visualisation pynn",

    # Requirements
    install_requires=[
        'PyOpenGL',
        'SpiNNUtilities == 1!6.0.0',
        'SpiNNFrontEndCommon == 1!6.0.0',
    ],
    extras_require={
        "acceleration": ["PyOpenGL_accelerate"]
    },

    # Scripts
    entry_points={
        "gui_scripts": [
            "spynnaker_raytrace = spynnaker_visualisers.raytrace.drawer:main",
            "spynnaker_sudoku = "
            "spynnaker_visualisers.sudoku.sudoku_visualiser:main",
        ],
    },
    maintainer="SpiNNakerTeam",
    maintainer_email="spinnakerusers@googlegroups.com"
)
