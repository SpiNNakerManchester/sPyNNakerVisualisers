# https://stackoverflow.com/questions/63475461/unable-to-import-opengl-gl-in-python-on-macos

""" Hack for macOS Big Sur """
from ctypes import util


def patch_ctypes():
    orig_util_find_library = util.find_library

    def new_util_find_library(name):
        res = orig_util_find_library(name)
        if res:
            return res
        return f'/System/Library/Frameworks/{name}.framework/{name}'

    util.find_library = new_util_find_library
