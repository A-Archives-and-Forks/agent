# -*- coding: utf-8 -*-

'''
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
'''

import os
import sys
import shutil
import logging.handlers

def is_py2():
    return sys.version_info[0]==2

if is_py2():
    def _py2_str_new(o):
        if isinstance(o, unicode):
            return o 
        elif isinstance(o, str):
            return o.decode("utf8", errors="replace")
        else:
            return str(o).decode("utf8", errors="replace")
    str_new=_py2_str_new    
else:
    str_new=str

def _write_err(s,e):
    bamsg=False;
    try:
        if len(e.message)>0:
            bamsg=True;
    except:
        None
    try:
        appmsg=None
        if bamsg:
            appmsg=str_new(e.message)
        else:
            appmsg=str_new(e)
        print(s + ":" + appmsg)
    except:
        print(s + ": Unexpected error.")

def _write_info(s):
    print(s)

def _write_status_file(st):
    try:
        with open("updater.status", 'w') as f:
            f.write(st)
        return True
    except:
        if os.path.exists("updater.status"):
            os.remove("updater.status")
        return False

def update_dir(pts,ptd,pto=None):
    if pto is not None:
        os.mkdir(pto)
    if ptd!="" and not os.path.exists(ptd):
        os.mkdir(ptd)
    flst = os.listdir(pts)
    for fn in flst:
        fsrc=pts+os.sep+fn
        fold=None
        if pto is not None:
            fold=pto+os.sep+fn
        if ptd!="":
            fdst=ptd+os.sep
        else:
            fdst=""
        fdst+=fn
        _write_info("Updating " + fdst + "...")
        if os.path.isdir(fsrc):
            update_dir(fsrc,fdst,fold)
        else:
            if os.path.exists(fdst):
                if fold is not None:
                    shutil.move(fdst,fold)
                else:
                    os.remove(fdst)
                #Extra check
                if os.path.exists(fdst):
                    raise Exception("Remove file " + fdst)
            shutil.move(fsrc, fdst)
            #Extra check
            if not os.path.exists(fdst):
                raise Exception("Move file " + fdst)

def restore_dir(pts,ptd):
    flst = os.listdir(pts)
    if ptd!="" and not os.path.exists(ptd):
        os.mkdir(ptd)        
    for fn in flst:
        fsrc=pts+os.sep+fn
        if ptd!="":
            fdst=ptd+os.sep
        else:
            fdst=""
        fdst+=fn
        _write_info("Restoring " + fdst + "...")
        if os.path.isdir(fsrc):
            restore_dir(fsrc,fdst)
        else:
            if os.path.exists(fdst):
                os.remove(fdst)
            shutil.move(fsrc, fdst)            
    
def _check_and_restore():
    if os.path.exists("updateOLD"):
        try:
            _write_info("Begin restore")
            restore_dir("updateOLD","")
            shutil.rmtree("updateOLD")
            _write_info("End restore")
        except Exception as ex:
            _write_err("Error restore",ex)
            return False
    return True

def _fix_old_installation():
    #03/11/2021
    try:    
        iver=0
        if os.path.exists("native" + os.sep + "installer.ver"):
            with open("native" + os.sep + "installer.ver", 'r') as f:
                try:
                    iver=int(f.read())
                except:
                    None
        if iver<=0:
            if os.path.exists("agent.py"):
                _write_info("Compiling...")
                import compileall
                if is_py2():
                    compileall.compile_dir(".", 0, ddir=".", quiet=1)
                else:
                    compileall.compile_dir(".", 0, ddir=".", quiet=1, legacy=True)
    except Exception as ex:
        _write_err("Error fix old installation 03/11/2021",ex)
    
    #05/08/2025
    try:
        import platform
        if platform.system().lower() == "linux" and os.path.lexists("runtime/lib/libz.so.1"):
            import glob
            pathslibz = [
            "/lib/*/libz.so.1",
            "/usr/lib/*/libz.so.1",
            "/lib/libz.so.1",
            "/usr/lib/libz.so.1",
            "/lib64/libz.so.1",
            "/usr/lib64/libz.so.1"
            ]
            pathlibz=None
            for pth in pathslibz:
                matches = glob.glob(pth)
                if matches:
                    pathlibz=matches[0]
                    break
            if pathlibz is not None:
                for pthcnf in ["native/dwagent","native/systray"]:
                    if os.path.exists(pthcnf):
                        bmod = True
                        with open(pthcnf, 'r') as f:
                            lns = f.readlines()
                            for i, l in enumerate(lns):
                                if "export LD_PRELOAD=" in l:
                                    bmod=False
                            if bmod:
                                bmod=False
                                for i, l in enumerate(lns):
                                    if "export LD_LIBRARY_PATH=" in l:
                                        lns.insert(i+1, "export LD_PRELOAD={}\n".format(pathlibz))
                                        bmod=True
                                        break
                        if bmod:
                            with open(pthcnf, 'w') as f:
                                f.writelines(lns)
                                _write_info("Fixed file " + pthcnf + ": added LD_PRELOAD.")
    except Exception as ex:
        _write_err("Error fix old installation 05/08/2025",ex)
    

class LoggerStdRedirect(object):
    
    def __init__(self,lg,lv):
        self._logger = lg;
        self._level = lv;
        
    def write(self, data):
        try:
            for line in data.rstrip().splitlines():
                self._logger.log(self._level, line.rstrip())
        except:
            None
    
    def flush(self):
        None

if __name__ == "__main__":
    if _write_status_file("INPROGRESS"):
        try:
            lgr = logging.getLogger()
            hdlr = logging.handlers.RotatingFileHandler("dwagent.log", "a", 1000000, 3)
            formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s")
            hdlr.setFormatter(formatter)
            lgr.addHandler(hdlr)
            lgr.setLevel(logging.INFO)
            sys.stdout=LoggerStdRedirect(lgr,logging.INFO)
            sys.stderr=LoggerStdRedirect(lgr,logging.INFO)
        except Exception as ex:
            _write_err("Error create log",ex)
        if not _check_and_restore():
            _write_status_file("ERROR")
        else:
            if os.path.exists("update"):
                _write_info("Begin update")            
                try:
                    update_dir("update","","updateOLD")
                    shutil.rmtree("update")
                    shutil.rmtree("updateOLD")
                    _fix_old_installation()
                    _write_status_file("OK")
                except Exception as ex:
                    _write_status_file("FAILED")
                    _write_err("Failed update",ex)
                finally:
                    if _check_and_restore():
                        if os.path.exists("update"):
                            shutil.rmtree("update")
                    else:
                        _write_status_file("ERROR")
                _write_info("End update")
            else:
                _write_status_file("OK")

