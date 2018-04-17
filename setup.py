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
    author="Donal Fellows",
    description="Visualisation clients for special sPyNNaker networks.",
    license="GPLv2",
    classifiers=[
        "Development Status :: 3 - Alpha",

        "Intended Audience :: Developers",
        "Intended Audience :: Science/Research",

        "License :: OSI Approved :: GNU General Public License v2 (GPLv2)",

        "Operating System :: POSIX :: Linux",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: MacOS",

        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.4",
        "Programming Language :: Python :: 3.5",
    ],
    keywords="spinnaker visualisation pynn",

    # Requirements
    install_requires=[
        'six >= 1.8.0',
        'PyOpenGL',
        'SpiNNUtilities >= 1!4.0.0, < 1!5.0.0',
        'SpiNNFrontEndCommon >= 1!4.0.0, < 1!5.0.0',
    ],
    extras_require={
        "acceleration": ["PyOpenGL_accelerate"]
    },

    # Scripts
    entry_points={
        "gui_scripts": [
            "spynnaker_sudoku = "
            "spynnaker_visualisers.sudoku.sudoku_visualiser:main",
        ],
    }
)
