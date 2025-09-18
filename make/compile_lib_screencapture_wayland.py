# -*- coding: utf-8 -*-
'''
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
'''
import compile_generic

class Compile(compile_generic.Compile):
    
    def __init__(self):
        compile_generic.Compile.__init__(self,"lib_screencapture","wayland")
    
    def get_os_config(self,osn):
        conf=None
        if osn=="linux":
            conf={}
            conf["outname"]="dwagscreencapturewayland.so" 
            conf["cpp_include_paths"]=["/usr/include/dbus-1.0","/usr/lib/dbus-1.0/include","/usr/include/pipewire-0.3","/usr/include/spa-0.2"] 
            #conf["cpp_library_paths"]=""
            conf["libraries"]=["dbus-1", "pipewire-0.3"]
            conf["cpp_compiler_flags"]="-DOS_WAYLAND"
        return conf
    

if __name__ == "__main__":    
    m = Compile()
    m.run()
    
    
    
    
    
    