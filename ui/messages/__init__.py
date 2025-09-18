# -*- coding: utf-8 -*-

'''
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
'''
import resources

restext=resources.ResText("ui.messages")

def set_locale(lng):
    restext.set_locale(lng)
    
def get_locale():
    return restext.get_locale()

def is_rtl():
    try:
        l=restext.get_locale()
        if l is not None:
            if l=="iw" or l.startswith("iw_"):
                return True
    except:
        None
    return False

def get_message(key):
    return restext.get(key)