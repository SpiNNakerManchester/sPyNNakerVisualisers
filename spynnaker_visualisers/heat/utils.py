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
