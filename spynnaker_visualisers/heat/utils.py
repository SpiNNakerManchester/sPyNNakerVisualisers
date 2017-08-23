import datetime
from spynnaker_visualisers.heat.constants import NOTDEFINED


def clamp(low, value, high):
    if value < low:
        return low
    if value > high:
        return high
    return value


def is_defined(f):
    return f > NOTDEFINED + 1


def timestamp():
    td = datetime.datetime.now()
    return td.microsecond + (td.second + td.day * 86400) * 10**6
