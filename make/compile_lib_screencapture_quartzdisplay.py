# -*- coding: utf-8 -*-
'''
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
'''
import compile_generic

class Compile(compile_generic.Compile):
    
    def __init__(self):
        compile_generic.Compile.__init__(self,"lib_screencapture","quartz display")
    
    def get_os_config(self,osn):
        conf=None
        if osn=="mac":
            conf={}
            conf["outname"]="dwagscreencapturequartzdisplay.dylib" 
            conf["frameworks"]=["ApplicationServices","SystemConfiguration","IOKit","Carbon","AppKit","CoreMedia","CoreGraphics","CoreVideo","AVFoundation", "IOSurface"]
            conf["cpp_compiler_flags"]="-DOS_QUARZDISPLAY -mmacosx-version-min=10.6"
            conf["linker_flags"]="-mmacosx-version-min=10.6"
        return conf
    

if __name__ == "__main__":    
    m = Compile()
    #m.set_arch(compile_generic.ARCH_X86_64)
    #m.set_arch(compile_generic.ARCH_X86_32)
    m.run()
    
    
    
    
    
    