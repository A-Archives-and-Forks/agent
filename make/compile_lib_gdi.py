# -*- coding: utf-8 -*-

'''
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
'''
import compile_generic

class Compile(compile_generic.Compile):
    
    def __init__(self):
        compile_generic.Compile.__init__(self,"lib_gdi")

    def get_os_config(self,osn):
        conf=None
        if osn=="windows":
            conf={}
            conf["outname"]="dwaggdi.dll" 
            conf["cpp_include_paths"]=[] 
            conf["cpp_library_paths"]=conf["cpp_include_paths"]
            conf["cpp_compiler_flags"]="-O2" #USED -O2 BECAUSE WITH -O3 DRWEB ANTIVIRUS MARK FILE AS NOT TRUST
            conf["libraries"]=["gdi32", "shell32", "user32", "userenv"]
            conf["linker_flags"]="-static-libgcc -static-libstdc++ -shared"
        elif osn=="linux":
            conf={}
            conf["outname"]="dwaggdi.so" 
            conf["cpp_include_paths"]=["/usr/include/freetype2"]
            conf["cpp_library_paths"]=conf["cpp_include_paths"]
            conf["libraries"]=["X11", "Xpm","Xft"]
        elif osn=="mac":
            None
            conf={}
            conf["outname"]="dwaggdi.dylib" 
            conf["cpp_include_paths"]=[] 
            conf["cpp_library_paths"]=conf["cpp_include_paths"]
            conf["frameworks"]=["Cocoa","ApplicationServices","IOKit","SystemConfiguration"]
            conf["cpp_compiler_flags"]="-mmacosx-version-min=10.6"
            conf["linker_flags"]="-mmacosx-version-min=10.6"            
        return conf

if __name__ == "__main__":
    m = Compile()
    #m.set_arch(compile_generic.ARCH_X86_64)
    #m.set_arch(compile_generic.ARCH_X86_32)
    m.run()
    
    
    
    
    
    
    