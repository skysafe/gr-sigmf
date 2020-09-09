#
# Copyright 2008,2009 Free Software Foundation, Inc.
#
# This application is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This application is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

# The presence of this file turns this directory into a Python package

'''
GNU Radio tools for the Signal Metadata Format (SigMF).
'''

import os.path
from gnuradio import uhd


# Prepare gr_sigmf swig module to allow passing a string for device_addr_t.
def _prepare_sigmf_swig():
    try:
        from . import gr_sigmf_swig
    except ImportError:
        # This path is for running make test without an installed package.
        import os
        dirname, filename = os.path.split(os.path.abspath(__file__))
        __path__.append(os.path.join(dirname, "..", "..", "swig"))
        try:
            from . import gr_sigmf_swig
        except ImportError:
            # This path is for running pytest with an installed package.
            __path__.append(os.path.join(dirname, "..", "build", "swig"))
            from . import gr_sigmf_swig

    old_constructor = gr_sigmf_swig.usrp_gps_message_source

    def constructor_interceptor(*args, **kwargs):
        args = list(args)
        kwargs = dict(kwargs)
        if len(args) > 0:
            args[0] = uhd.device_addr_t(args[0])
        if kwargs.has_key('uhd_args'):
            kwargs['uhd_args'] = device_addr(kwargs['uhd_args'])
        # Don't pass kwargs, it confuses swig, instead map into args list.
        for key in ('uhd_args', 'poll_interval'):
            if kwargs.has_key(key):
                args.append(kwargs[key])
        return old_constructor(*args)

    setattr(gr_sigmf_swig, 'usrp_gps_message_source', constructor_interceptor)


_prepare_sigmf_swig()
from .gr_sigmf_swig import *
