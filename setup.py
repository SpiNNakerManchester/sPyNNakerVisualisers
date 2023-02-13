# Copyright (c) 2017-2023 The University of Manchester
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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

        "License :: OSI Approved :: Apache License 2.0",

        "Operating System :: POSIX :: Linux",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: MacOS",

        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
    ],
    keywords="spinnaker visualisation pynn",

    # Requirements
    install_requires=[
        'PyOpenGL',
        'SpiNNUtilities == 1!6.0.1',
        'SpiNNFrontEndCommon == 1!6.0.1',
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
