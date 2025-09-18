# -*- coding: utf-8 -*-
'''
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
'''
import compile_generic

class Compile(compile_generic.Compile):
    
    def __init__(self):
        compile_generic.Compile.__init__(self,"lib_screencapture","screencapurekit")
    
    def get_os_config(self,osn):
        conf=None
        if osn=="mac":
            conf={}
            conf["outname"]="dwagscreencapturekit.dylib" 
            conf["frameworks"]=["ApplicationServices","SystemConfiguration","IOKit","Carbon","AppKit","CoreMedia","CoreGraphics","CoreVideo","ScreenCaptureKit"]
            conf["cpp_compiler_flags"]="-DOS_SCREENCAPTUREKIT"
        return conf
    

if __name__ == "__main__":    
    m = Compile()
    #m.set_arch(compile_generic.ARCH_X86_32)
    #m.set_arch(compile_generic.ARCH_X86_64)
    m.run()
    
    
    
    
    
    