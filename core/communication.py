# -*- coding: utf-8 -*-
'''
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
'''

import ssl
import struct
import time
import socket
import threading
import xml.etree.ElementTree
import os
import math
import utils
import json
import mimetypes
import hashlib
import base64
import sys

BUFFER_SIZE_MAX = 65536
BUFFER_SIZE_MIN = 4096

SIZE_INTEGER=math.pow(2,32)
SIZE_LONG=math.pow(2,64)

_SOCKET_TIMEOUT_CONNECT = 15
_TRANSFER_TIMEOUT = 20

_cacerts_path="cacerts.pem"
_proxy_detected = {}
_proxy_detected["semaphore"]=threading.Condition()
_proxy_detected["check"] = False
_proxy_detected["info"] = None

def is_windows():
    return utils.is_windows()

def is_linux():
    return utils.is_linux()

def is_mac():
    return utils.is_mac()

def get_time():
    return utils.get_time()

def _is_ssl_create_default_context():
    if hasattr(ssl, 'SSLContext'):
        if sys.version_info >= (3, 7) and hasattr(ssl, 'PROTOCOL_TLSv1_2'):
            return hasattr(ssl, 'create_default_context')
    return False

def get_ssl_info():
    sslret=ssl.OPENSSL_VERSION + " ("
    if _is_ssl_create_default_context():
        sslret += "TLSv1.2+"
    elif hasattr(ssl, 'PROTOCOL_TLSv1_2'):
        sslret += "TLSv1.2" 
    elif hasattr(ssl, 'PROTOCOL_TLSv1_1'):
        sslret += "TLSv1.1"
    elif hasattr(ssl, 'PROTOCOL_TLSv1'):
        sslret += "TLSv1"
    else:
        sslret += "Unknown"
    sslret += ")"
    return sslret

def _get_ssl_ver():
    if hasattr(ssl, 'PROTOCOL_TLSv1_2'):
        return ssl.PROTOCOL_TLSv1_2 
    if hasattr(ssl, 'PROTOCOL_TLSv1_1'):
        return ssl.PROTOCOL_TLSv1_1
    if hasattr(ssl, 'PROTOCOL_TLSv1'):
        return ssl.PROTOCOL_TLSv1
    if hasattr(ssl, 'PROTOCOL_TLS'):
        return ssl.PROTOCOL_TLS
    return ssl.PROTOCOL_SSLv23 #DEFAULT

def _connect_proxy_http(sock, host, port, proxy_info):
    usr = proxy_info.get_user()
    pwd = proxy_info.get_password()
    arreq=[]
    arreq.append("CONNECT %s:%d HTTP/1.0" % (host, port))
    if usr is not None and len(usr)>0:
        auth=utils.bytes_to_str(utils.enc_base64_encode(utils.str_to_bytes(usr + ":" + pwd,"utf8")))
        arreq.append("\r\nProxy-Authorization: Basic %s" % (auth))
    arreq.append("\r\n\r\n")
    sock.sendall(utils.str_to_bytes("".join(arreq)))
    resp = Response(sock)
    if resp.get_code() != '200':
        raise Exception("Proxy http error: " + str(resp.get_code()) + ".")
    

def _connect_proxy_socks(sock, host, port, proxy_info):
    usr = proxy_info.get_user()
    pwd = proxy_info.get_password()
    if proxy_info.get_type()=='SOCKS5':
        arreq = []
        arreq.append(struct.pack(">BBBB", 0x05, 0x02, 0x00, 0x02))
        sock.sendall(utils.bytes_join(arreq))
        resp = sock.recv(2)
        ver = utils.bytes_get(resp,0)
        mth = utils.bytes_get(resp,1)
        if ver!=0x05:
            raise Exception("Proxy socks error: Incorrect version.")
        if mth!=0x00 and mth!=0x02:
            raise Exception("Proxy socks error: Method not supported.")
        if mth==0x02:
            if usr is not None and len(usr)>0 and pwd is not None and len(pwd)>0:
                arreq = []
                arreq.append(struct.pack(">B", 0x01))
                arreq.append(struct.pack(">B", len(usr)))
                for c in usr:
                    arreq.append(struct.pack(">B", ord(c)))
                arreq.append(struct.pack(">B", len(pwd)))
                for c in pwd:
                    arreq.append(struct.pack(">B", ord(c)))                
                sock.sendall(utils.bytes_join(arreq))
                resp = sock.recv(2)
                ver = utils.bytes_get(resp,0)
                status = utils.bytes_get(resp,1)
                if ver!=0x01 or status != 0x00:
                    raise Exception("Proxy socks error: Incorrect Authentication.")
            else:
                raise Exception("Proxy socks error: Authentication required.")
        arreq = []
        arreq.append(struct.pack(">BBB", 0x05, 0x01, 0x00))
        try:
            addr_bytes = socket.inet_aton(host)
            arreq.append(b"\x01")
            arreq.append(addr_bytes)
        except socket.error:
            arreq.append(b"\x03")
            arreq.append(struct.pack(">B", len(host)))
            for c in host:
                arreq.append(struct.pack(">B", ord(c)))
        arreq.append(struct.pack(">H", port))
        sock.sendall(utils.bytes_join(arreq))
        resp = sock.recv(1024)
        ver = utils.bytes_get(resp,0)
        status = utils.bytes_get(resp,1)
        if ver!=0x05 or status != 0x00:
            raise Exception("Proxy socks error.")
    else:
        remoteresolve=False
        try:
            addr_bytes = socket.inet_aton(host)
        except socket.error:
            if proxy_info.get_type()=='SOCKS4A':
                addr_bytes = b"\x00\x00\x00\x01"
                remoteresolve=True
            else:
                addr_bytes = socket.inet_aton(socket.gethostbyname(host))
            
        arreq = []
        arreq.append(struct.pack(">BBH", 0x04, 0x01, port))
        arreq.append(addr_bytes)
        if usr is not None and len(usr)>0:
            for c in usr:
                arreq.append(struct.pack(">B", ord(c)))
        arreq.append(b"\x00")
        if remoteresolve:
            for c in host:
                arreq.append(struct.pack(">B", ord(c)))
            arreq.append(b"\x00")
        sock.sendall(utils.bytes_join(arreq))
        
        resp = sock.recv(8)
        if len(resp)<2:
            raise Exception("Proxy socks error.")
        if utils.bytes_get(resp,0) != 0x00:
            raise Exception("Proxy socks error.")
        status = utils.bytes_get(resp,1)
        if status != 0x5A:
            raise Exception("Proxy socks error.")

def _detect_proxy_windows():
    prxi=None
    try:
        sproxy=None
        import _winreg
        aReg = _winreg.ConnectRegistry(None,_winreg.HKEY_CURRENT_USER)
        aKey = _winreg.OpenKey(aReg, r"Software\Microsoft\Windows\CurrentVersion\Internet Settings")
        try: 
            subCount, valueCount, lastModified = _winreg.QueryInfoKey(aKey)
            penabled=False
            pserver=None
            for i in range(valueCount):                                           
                try:
                    n,v,t = _winreg.EnumValue(aKey,i)
                    if n.lower() == 'proxyenable':
                        penabled = v and True or False
                    elif n.lower() == 'proxyserver':
                        pserver = v
                except EnvironmentError:                                               
                    break
            if penabled and pserver is not None:
                sproxy=pserver
        finally:
            _winreg.CloseKey(aKey)   
        if sproxy is not None:
            stp=None
            sho=None
            spr=None            
            lst = sproxy.split(";")
            for v in lst:
                if len(v)>0:
                    ar1 = v.split("=")
                    if len(ar1)==1:
                        stp="HTTP"
                        ar2 = ar1[0].split(":")
                        sho=ar2[0]
                        spr=ar2[1]
                        break
                    elif ar1[0].lower()=="http":
                        stp="HTTP"
                        ar2 = ar1[1].split(":")
                        sho=ar2[0]
                        spr=ar2[1]
                        break
                    elif ar1[0].lower()=="socks":
                        stp="SOCKS5"
                        ar2 = ar1[1].split(":")
                        sho=ar2[0]
                        spr=ar2[1]
                    
            if stp is not None:
                prxi = ProxyInfo()
                prxi.set_type(stp)
                prxi.set_host(sho)
                prxi.set_port(int(spr))
                #print("PROXY WINDOWS DETECTED:" + stp + "  " + spr)
                
    except:
        None
    return prxi

def _detect_proxy_linux():
    prxi=None
    try:
        sprx=None
        sprx=os.getenv("all_proxy")
        if "http_proxy" in os.environ:
            sprx = os.environ["http_proxy"]
        elif "all_proxy" in os.environ:
            sprx = os.environ["all_proxy"]
        if sprx is not None:
            stp=None
            if sprx.endswith("/"):
                sprx=sprx[0:len(sprx)-1]            
            if sprx.lower().startswith("socks:"):
                stp="SOCKS5"
                sprx=sprx[len("socks:"):]
            elif sprx.lower().startswith("http:"):
                stp="HTTP"
                sprx=sprx[len("http:"):]
            if stp is not None:
                sun=None
                spw=None
                sho=None
                spr=None
                ar = sprx.split("@")
                if len(ar)==1:
                    ar1 = sprx[0].split(":")
                    sho=ar1[0]
                    spr=ar1[1]
                else: 
                    ar1 = sprx[0].split(":")
                    sun=ar1[0]
                    spw=ar1[1]
                    ar2 = sprx[1].split(":")
                    sho=ar2[0]
                    spr=ar2[1]
                prxi = ProxyInfo()
                prxi.set_type(stp)
                prxi.set_host(sho)
                prxi.set_port(int(spr))
                prxi.set_user(sun)
                prxi.set_password(spw)
    except:
        None
    return prxi

def release_detected_proxy():
    global _proxy_detected
    _proxy_detected["semaphore"].acquire()
    try:
        _proxy_detected["check"]=False
        _proxy_detected["info"]=None
    finally:
        _proxy_detected["semaphore"].release()

def _set_detected_proxy_none():
    global _proxy_detected
    _proxy_detected["semaphore"].acquire()
    try:
        _proxy_detected["check"]=True
        _proxy_detected["info"]=None
    finally:
        _proxy_detected["semaphore"].release()
    
def set_cacerts_path(path):
    global _cacerts_path
    _cacerts_path=path

def _connect_socket(host, port, proxy_info, opts=None):
    timeout=_SOCKET_TIMEOUT_CONNECT
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)        
        if opts is not None and "rcvbuf" in opts:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, opts["rcvbuf"])
        if opts is not None and "sndbuf" in opts:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, opts["sndbuf"])
            
        if opts is not None and "timeout" in opts:
            timeout=opts["timeout"]
        sock.settimeout(timeout)
        bprxdet=False
        prxi=proxy_info
        if prxi is None or prxi.get_type() is None or proxy_info.get_type()=='SYSTEM':
            global _proxy_detected
            _proxy_detected["semaphore"].acquire()
            try:
                if not _proxy_detected["check"]:
                    try:
                        if is_windows():
                            _proxy_detected["info"] = _detect_proxy_windows()
                        elif is_linux():
                            _proxy_detected["info"] = _detect_proxy_linux()
                        elif is_mac():
                            _proxy_detected["info"]=None
                    except:
                        _proxy_detected=None
                if _proxy_detected is not None:
                    bprxdet=True
                    prxi = _proxy_detected["info"]
                _proxy_detected["check"]=True
            finally:
                _proxy_detected["semaphore"].release()
            
        conn_ex=None    
        func_prx=None
        if prxi is None or prxi.get_type() is None or prxi.get_type()=='NONE':
            sock.connect((host, port))
        elif prxi.get_type()=='HTTP':
            try:
                sock.connect((prxi.get_host(), prxi.get_port()))
                func_prx=_connect_proxy_http
            except:
                conn_ex=utils.get_exception()
        elif prxi.get_type()=='SOCKS4' or prxi.get_type()=='SOCKS4A' or prxi.get_type()=='SOCKS5':
            try:
                sock.connect((prxi.get_host(), prxi.get_port()))
                func_prx=_connect_proxy_socks
            except:
                conn_ex=utils.get_exception()
        else:
            sock.connect((host, port))
        
        if func_prx is not None:
            try:
                func_prx(sock, host, port, prxi)
            except:
                conn_ex=utils.get_exception()
        
        if conn_ex is not None:
            if bprxdet:
                try:
                    release_detected_proxy()
                    sock.close()
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                    sock.settimeout(timeout)
                    sock.connect((host, port)) #TRY TO CONNECT WITHOUT PROXY
                    _set_detected_proxy_none()
                    bprxdet=False
                except:
                    raise conn_ex
            else:
                raise conn_ex
                
        
        while True:
            try:
                #VALIDA CERITFICATI
                global _cacerts_path
                if hasattr(ssl, 'SSLContext'):
                    if _is_ssl_create_default_context():
                        if _cacerts_path!="":
                            ctx = ssl.create_default_context(                                
                                cafile=_cacerts_path
                            )
                            ctx.verify_mode = ssl.CERT_REQUIRED
                            ctx.check_hostname = True
                            sock = ctx.wrap_socket(sock,server_hostname=host)
                        else:
                            ctx = ssl.create_default_context()
                            sock = ctx.wrap_socket(sock)
                    else:
                        ctx = ssl.SSLContext(_get_ssl_ver())
                        if _cacerts_path!="":
                            ctx.verify_mode = ssl.CERT_REQUIRED
                            ctx.check_hostname = True
                            ctx.load_verify_locations(_cacerts_path)
                            sock = ctx.wrap_socket(sock,server_hostname=host)
                        else:
                            sock = ctx.wrap_socket(sock)
                else:
                    iargs = None
                    try:
                        import inspect
                        iargs = inspect.getargspec(ssl.wrap_socket).args
                    except:                   
                        None
                    if iargs is not None and "cert_reqs" in iargs and "ca_certs" in iargs and _cacerts_path!="": 
                        sock = ssl.wrap_socket(sock, ssl_version=_get_ssl_ver(), cert_reqs=ssl.CERT_REQUIRED, ca_certs=_cacerts_path)
                    else:
                        sock = ssl.wrap_socket(sock, ssl_version=_get_ssl_ver())
                break
            except:
                conn_ex=utils.get_exception()
                if bprxdet:
                    if "CERTIFICATE_VERIFY_FAILED" in str(conn_ex):
                        try: 
                            release_detected_proxy()
                            sock.close()
                            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                            sock.settimeout(timeout)
                            sock.connect((host, port)) #TRY TO CONNECT WITHOUT PROXY
                            _set_detected_proxy_none()
                            bprxdet=False
                        except:
                            raise conn_ex
                    else:                        
                        raise conn_ex                    
                else:
                    raise conn_ex  
            
            
    except:
        e=utils.get_exception()
        sock.close()
        raise e
    sock.settimeout(None)
    return sock

def _close_socket(sock):
    try:                
        sock.shutdown(socket.SHUT_RDWR)
    except:
        None
    try:
        sock.close()
    except:
        None

def _is_content_type(ct,vl):
    ar = ct.split(";")
    for k in ar:
        if k.strip()==vl:
            return True    
    return False

def prop_to_xml(prp):
    ardata = []
    ardata.append('<!DOCTYPE properties SYSTEM "http://java.sun.com/dtd/properties.dtd">')
    root_element = xml.etree.ElementTree.Element("properties")
    for key in prp:
        child = xml.etree.ElementTree.SubElement(root_element, "entry")
        child.attrib['key'] = key
        child.text = prp[key]
    ardata.append(utils.bytes_to_str(xml.etree.ElementTree.tostring(root_element)))
    return ''.join(ardata)

def xml_to_prop(s):
    prp = {}
    root = xml.etree.ElementTree.fromstring(utils.buffer_new(s,0,len(s)))
    for child in root:
        prp[child.attrib['key']] = child.text
    return prp

def _split_utl(url):
    lnhttps = 8
    #detect host and port
    p=url[lnhttps:].find('/')
    host=url[lnhttps:lnhttps+p]
    port=443
    i=host.find(':')
    if i>=0:
        port=int(host[i+1:])
        host=host[:i]
    #detect path
    u = url[p+lnhttps:]
    return {'host':host,  'port':port,  'path':u}

def download_url_file(urlsrc, fdest, proxy_info=None, response_transfer_progress=None):
    sredurl=None
    sp = _split_utl(urlsrc)
    sock = _connect_socket(sp["host"], sp["port"], proxy_info)
    sock.settimeout(_TRANSFER_TIMEOUT) 
    try:
        req = Request("GET", sp["path"],  {'Host' : sp["host"] + ':' + str(sp["port"]),  'Connection' : 'close'})
        sock.sendall(req.to_message())
    
        #read response
        if utils.path_exists(fdest):
            utils.path_remove(fdest)
        ftmp = fdest + "TMP"
        if utils.path_exists(ftmp):
            utils.path_remove(ftmp)        
        resp = Response(sock, ftmp, response_transfer_progress)
        if resp.get_code() == '301' or resp.get_code() == '307':
            sredurl=resp.get_headers()["Location"]
        elif resp.get_code() != '200':
            raise Exception("Download error " + str(resp.get_code()) + ".")
    finally:
        sock.shutdown(1)
        sock.close()
    if sredurl is not None:
        return download_url_file(sredurl, fdest, proxy_info, response_transfer_progress)
    else:
        if utils.path_exists(ftmp):
            utils.path_move(ftmp, fdest)
    return urlsrc 

def get_url_prop(url, proxy_info=None):
    sredurl=None
    sp = _split_utl(url)    
    sock = _connect_socket(sp["host"], sp["port"], proxy_info)
    sock.settimeout(_TRANSFER_TIMEOUT)
    try:
        req = Request("GET", sp["path"],  {'Host' : sp["host"] + ':' + str(sp["port"]),  'Connection' : 'close'})
        sock.sendall(req.to_message())        
        prpresp = None
        resp = Response(sock)
        if resp.get_code() == '200':
            rtp="xml"
            try:
                hds = resp.get_headers()  
                if hds is not None and "Content-Type" in hds:
                    if _is_content_type(hds["Content-Type"],"application/json"):
                        rtp="json"
            except:
                None
            if rtp=="json":
                prpresp = json.loads(resp.get_body())
            else:
                prpresp = xml_to_prop(resp.get_body())
        elif resp.get_code() == '301' or resp.get_code() == '307':
            sredurl=resp.get_headers()["Location"]
        else:
            raise Exception("Get url properties error " + str(resp.get_code()) + ".")
    finally:
        sock.shutdown(1)
        sock.close()
    if sredurl is not None:
        prpresp = get_url_prop(sredurl,proxy_info)
    return prpresp

def ping_url(url, proxy_info=None):
    tmret=None
    try:
        sp = _split_utl(url)
        sock = _connect_socket(sp["host"], sp["port"], proxy_info,{"timeout":2})
        sock.settimeout(2)
        try:
            req = Request("GET", sp["path"],  {'Host': sp["host"]+':'+str(sp["port"]),  'Connection': 'keep-alive'})
            sock.sendall(req.to_message())
            resp = Response(sock)
            if resp.get_code() == '200':
                tm=time.time()
                req = Request("GET", sp["path"],  {'Host': sp["host"]+':'+str(sp["port"]),  'Connection': 'close'})
                sock.sendall(req.to_message())
                resp = Response(sock)
                if resp.get_code() == '200':
                    tmret=round(time.time()-tm,3)
        finally:
            sock.shutdown(1)
            sock.close()
    except:
        None
    return tmret

class ProxyInfo:
    def __init__(self):
        self._type="None"
        self._host=None
        self._port=None
        self._user=None
        self._password=None
        
    def set_type(self, ptype):
        self._type=ptype
    
    def set_host(self, host):
        self._host=host
        
    def set_port(self, port):
        self._port=port
    
    def set_user(self,  user):
        self._user=user
    
    def set_password(self,  password):
        self._password=password
    
    def get_type(self):
        return self._type
    
    def get_host(self):
        return self._host
        
    def get_port(self):
        return self._port
    
    def get_user(self):
        return self._user
    
    def get_password(self):
        return self._password
        

class Request:
    def __init__(self, method, url, prp=None):
        self._method = method
        self._url = url
        self._prp = prp
        self._body = None

    def set_body(self, body):
        self._body = body

    def to_message(self):
        arhead = []
        arhead.append(self._method)
        arhead.append(' ')
        arhead.append(self._url)
        arhead.append(' ')
        arhead.append('HTTP/1.1')
        if self._prp is not None:
            for k in self._prp:
                arhead.append('\r\n')
                arhead.append(k)
                arhead.append(': ')
                arhead.append(self._prp[k])
            
        if self._body is not None:
            arhead.append('\r\n')
            arhead.append('Compression: zlib')
            arhead.append('\r\n')
            arhead.append('Content-Length: ')
            arhead.append(str(len(self._body)))
        arhead.append('\r\n')
        arhead.append('\r\n')
        if self._body is not None:
            arhead.append(self._body)
        return utils.str_to_bytes(''.join(arhead))

class Response_Transfer_Progress:
    
    def __init__(self, events=None):
            self._on_data=None
            self._properties={}
            self._byte_transfer=0
            self._byte_length=0
            if events is not None:
                if 'on_data' in events:
                    self._on_data=events['on_data']
    
    def set_property(self, key, value):
        self._properties[key]=value
    
    def get_property(self, key):
        if key not in self._properties:
            return None
        return self._properties[key]
    
    def get_byte_transfer(self):
        return self._byte_transfer
    
    def get_byte_length(self):
        return self._byte_length
    
    def fire_on_data(self,  byte_transfer,  byte_length):
        self._byte_transfer=byte_transfer
        self._byte_length=byte_length
        if self._on_data is not None:
            self._on_data(self)

class Response:
    def __init__(self, sock, body_file_name=None,  response_transfer_progress=None):
        data = utils.bytes_new(0)
        while utils.bytes_to_str(data).find('\r\n\r\n') == -1:
            app=sock.recv(1024 * 4)
            if app is None or len(app)==0:
                raise Exception('Close connection')
            data += app 
        ar = utils.bytes_to_str(data).split('\r\n\r\n')
        head = ar[0].split('\r\n')
        appbody = []
        appbody.append(data[len(ar[0])+4:])
        self._code = None
        self._headers = {}
        clenkey=None
        for item in head:
            if self._code is None:
                self._code = item.split(' ')[1]
            else:
                apppos = item.index(':')
                appk=item[0:apppos].strip()
                if appk.lower()=="content-length":
                    clenkey=appk
                self._headers[appk] = item[apppos+1:].strip()
        #Read body
        if self._code != '301' and self._code != '307' and clenkey is not None:
            self._extra_data=None
            lenbd = int(self._headers[clenkey])
            fbody=None
            try:
                jbts=utils.bytes_join(appbody)
                if body_file_name is not None:
                    fbody=utils.file_open(body_file_name, 'wb')
                    fbody.write(jbts)
                cnt=len(jbts)
                if response_transfer_progress is not None:
                    response_transfer_progress.fire_on_data(cnt,  lenbd)
                szbuff=1024*2
                buff=None
                while lenbd > cnt:
                    buff=sock.recv(szbuff)
                    if buff is None or len(buff)==0:
                        break
                    cnt+=len(buff)
                    if response_transfer_progress is not None:
                        response_transfer_progress.fire_on_data(cnt,  lenbd)
                    if body_file_name is None:
                        appbody.append(buff)
                    else:
                        fbody.write(buff)
            finally:
                if fbody is not None:
                    fbody.close()
                else:
                    self._body=utils.bytes_join(appbody)
        else:
            self._extra_data=utils.bytes_join(appbody)
            if len(self._extra_data)==0:
                self._extra_data=None

    def get_extra_data(self):
        return self._extra_data

    def get_code(self):
        return self._code

    def get_headers(self):
        return self._headers
    
    def get_body(self):
        return self._body


class Worker(threading.Thread):
    
    def __init__(self, parent,  queue, i):
        self._parent = parent
        threading.Thread.__init__(self, name=self._parent.get_name() + "_" + str(i))
        self.daemon=True
        self._queue=queue
        
    def run(self):
        while not self._parent._destroy:
            func, args, kargs = self._queue.get()
            if func is not None:
                try: 
                    func(*args, **kargs)
                except: 
                    e=utils.get_exception()
                    self._parent.fire_except(e)
                self._queue.task_done()

class ThreadPool():
    
    def __init__(self, name, queue_size, core_size , fexcpt):
            self._destroy=False
            self._name=name
            self._fexcpt=fexcpt
            self._queue = utils.Queue(queue_size)
            for i in range(core_size):
                self._worker = Worker(self, self._queue, i)
                self._worker.start()
    
    def get_name(self):
        return self._name 

    def fire_except(self, e):
        if self._fexcpt is not None:
            self._fexcpt(e)

    def execute(self, func, *args, **kargs):
        if not self._destroy:
            self._queue.put([func, args, kargs])
    
    def destroy(self):
        self._destroy=True #DA GESTIRE


class QueueTask():
    
    def __init__(self, tpool):
        self._task_pool=tpool
        self._semaphore = threading.Condition()
        self.list = []
        self.running = False
        
    
    def _exec_func(self):
        while True:
            func = None
            self._semaphore.acquire()
            try:
                if len(self.list)==0:
                    self.running = False
                    break
                func = self.list.pop(0)
            finally:
                self._semaphore.release()
            func()
                        
        
    def execute(self, f, only_if_empty=False):
        self._semaphore.acquire()
        try:
            if not self.running:
                self.list.append(f)
                self.running=True
                self._task_pool.execute(self._exec_func)
            else:
                if only_if_empty:
                    if len(self.list)<2: #con < 2 sono sicuro che almeno l'ultimo viene eseguito
                        self.list.append(f)
                else:
                    self.list.append(f)
        finally:
            self._semaphore.release()
        
        
            
class BandwidthCalculator:
    
    def __init__(self, ckint=0.5, ccint=5.0):
        self._semaphore = threading.Condition()
        self._current_byte_transfered=0
        self._last_byte_transfered=0
        self._last_time=0
        self._bps=0
        self._buffer_size=BUFFER_SIZE_MIN
        self._check_intervall=ckint
        self._calc_intervall=ccint
        self._calc_ar=[]
        self._calc_elapsed=0
        self._calc_transfered=0
    
    def set_check_intervall(self,i):
        self._semaphore.acquire()
        try:
            self._check_intervall=i
        finally:
            self._semaphore.release()
    
    def get_check_intervall(self):
        self._semaphore.acquire()
        try:
            return self._check_intervall
        finally:
            self._semaphore.release()
            
    def add(self, c):
        self._semaphore.acquire()
        try:
            self._current_byte_transfered += c
            self._calculate()
        finally:
            self._semaphore.release()
    
    def _calculate(self):
        tm=get_time() 
        transfered=self._current_byte_transfered-self._last_byte_transfered
        elapsed = (tm - self._last_time)
        if elapsed<0:
            elapsed=0
            self._last_time=tm
        if elapsed>self._check_intervall:
            self._calc_ar.append({"elapsed":elapsed, "transfered":transfered})
            self._calc_elapsed+=elapsed
            self._calc_transfered+=transfered
            while len(self._calc_ar)>1 and self._calc_elapsed>self._calc_intervall:
                ar = self._calc_ar.pop(0)
                self._calc_elapsed-=ar["elapsed"]
                self._calc_transfered-=ar["transfered"]
            self._bps=int(float(self._calc_transfered)*(1.0/self._calc_elapsed))
            self._calculate_buffer_size()
            self._last_time=tm
            self._last_byte_transfered=self._current_byte_transfered        
    
    def get_bps(self):
        return self._bps
    
    def get_buffer_size(self):
        return self._buffer_size
    
    def _calculate_buffer_size(self):
        self._buffer_size=int(0.2*float(self._bps))
        if self._buffer_size<BUFFER_SIZE_MIN:
            self._buffer_size=BUFFER_SIZE_MIN
        elif self._buffer_size>BUFFER_SIZE_MAX:
            self._buffer_size=BUFFER_SIZE_MAX
        else:
            self._buffer_size=int((float(self._buffer_size)/512.0)*512.0)
        
    
    def get_transfered(self):
        return self._current_byte_transfered        

'''
class BandwidthLimiter:
    
    def __init__(self,sync=True):
        if sync:
            self._semaphore = threading.Condition()
        else:
            self._semaphore = None
        self._last_time=0
        self._bandlimit=0
        self._last_wait=0
        self._buffsz=0        
        self.set_bandlimit(0)
     
     
    def _semaphore_acquire(self):
        if self._semaphore is not None:
            self._semaphore.acquire()
    
    def _semaphore_release(self):
        if self._semaphore is not None:
            self._semaphore.release()
    
    def get_bandlimit(self):
        self._semaphore_acquire()
        try:
            return self._bandlimit
        finally:
            self._semaphore_release()
        
    def set_bandlimit(self,pbps):
        self._semaphore_acquire()
        try:
            if self._bandlimit==pbps:
                return
            if pbps>0:
                self._bandlimit=pbps
                self._buffsz=calculate_buffer_size(pbps)
            else:
                self._bandlimit=0
                self._buffsz=BUFFER_SIZE_MAX
        finally:
            self._semaphore_release()
        
    def get_buffer_size(self):
        self._semaphore_acquire()
        try:
            return self._buffsz
        finally:
            self._semaphore_release()
    
    def get_waittime(self, c):
        self._semaphore_acquire()
        try:
            tm=get_time() 
            timeout = 0
            if c > 0:
                if self._bandlimit > 0:
                    if tm>=self._last_time:
                        elapsed = (tm - self._last_time) - self._last_wait
                        maxt = float(self._bandlimit)*elapsed
                        timeout = float(c-maxt)/float(self._bandlimit) 
                        self._last_wait=timeout
                        if self._last_wait<-1.0:
                            self._last_wait=0.0
                        self._last_time=tm
                        if timeout < 0.0:
                            timeout=0.0
                    else:
                        self._last_time=tm 
                        self._last_wait=0.0
            return timeout
        finally:
            self._semaphore_release()
            
'''
    
'''
#############################
##### NEW COMMUNICATION #####
#############################
'''

class HttpConnectionSocketPoll(threading.Thread):
    IDLE_MAX=120.0
    
    def __init__(self):
        threading.Thread.__init__(self, name="HttpConnectionSocketPoll")
        self._lock=threading.RLock()
        self._list=[]
        self._destroy=False
    
    def release_socket(self, sock, host, port, proxy_info, timeout):
        with self._lock:
            if timeout<=0 or timeout>=HttpConnectionSocketPoll.IDLE_MAX:
                timeout=HttpConnectionSocketPoll.IDLE_MAX
            self._list.insert(0,{"sock":sock, "host":host, "port":port, "proxy_info":proxy_info, "timeout": timeout , "releasetime": time.time()})
        
    def get_socket(self, host, port, proxy_info):
        sock=None
        while True:
            with self._lock:
                for i in range(len(self._list)):
                    itm=self._list[i]
                    elps=time.time()-itm["releasetime"]
                    if elps>=0 or elps<=itm["timeout"]:
                        if itm["host"]==host and itm["port"]==port and itm["proxy_info"]==proxy_info:
                            sock=itm["sock"]
                            self._list.pop(i)
                            break
            
            if sock is None:                
                return _connect_socket(host, port, proxy_info)
            else:
                #CHECK CONNECTION
                sock.settimeout(0.0)
                try:
                    bt = sock.recv(4096)
                    _close_socket(sock)
                    sock=None
                except Exception as e:
                    None                
                if sock is not None:
                    sock.settimeout(None)
                    return sock 

    def destroy(self):
        self._destroy=True
    
    def run(self):
        while (not self._destroy):
            time.sleep(5)
            arsockclose=[]
            with self._lock:
                brem=True
                while brem:
                    brem=False 
                    for i in range(len(self._list)):
                        itm=self._list[i]
                        elps=time.time()-itm["releasetime"]
                        if elps<0 or elps>itm["timeout"]:
                            arsockclose.append(itm["sock"])
                            self._list.pop(i)
                            #print("REMOVE SOCKET timeout:"+str(itm["timeout"]))                            
                            brem=True
                            break
                    
                #print("SOCKET LIST COUNT:"+str(len(self._list)))
                                                
            for sk in arsockclose:
                _close_socket(sk)
            
            
            
        #CLOSE ALL SOCKET
        arsockclose=[]
        with self._lock:
            for itm in self._list:
                arsockclose.append(itm["sock"])
            self._list=[]
        for sk in arsockclose:
            _close_socket(sk)

def _fix_header_key(name):
    if name.startswith("dw_"): #DO NOT CHANGE dw_ headers
        return name                
    return "-".join([w.capitalize() for w in name.split("-")])

class HTTPHeaders():
    
    def __init__(self):
        self._dict = {}
    
    def __setitem__(self, name, value):
        nm = _fix_header_key(name)
        self._dict[nm] = value

    def __getitem__(self, name):
        return self._dict[_fix_header_key(name)]

    def __delitem__(self, name):
        nm = _fix_header_key(name)
        del self._dict[nm]
    
    def __len__(self):
        return len(self._dict)

    def __iter__(self):
        return iter(self._dict)
    
class HttpConnection():
    
    def __init__(self, mth, surl, hds, opts):
        self._method=mth
        self._url=surl
        self._request_headers=HTTPHeaders()
        if hds is not None:
            for k in hds:
                self._request_headers[k]=hds[k]
        self._proxy_info=None
        if "proxy_info" in opts:
            self._proxy_info=opts["proxy_info"]
        self._split_url = _split_utl(self._url)
        self._sock = None
        self._connected = False
        self._response_loaded = False
        self._response_code = None
        self._response_header = None
        self._response_length = 0
        self._response_tmpbody = None
        self._check_close_counter=None
        self._keep_alive=False
        self._keep_alive_timeout=0
        self._socket_pool=None
        if "http_socket_pool" in opts: 
            self._socket_pool=opts["http_socket_pool"]
    
    def connect(self):
        if self._connected:
            raise Exception("Already connected.")
        try:
            self._request_headers['Host']=self._split_url["host"] + ':' + str(self._split_url["port"])
            if self._socket_pool is not None:
                if not 'Connection' in self._request_headers:
                    self._request_headers['Connection']='keep-alive'
                self._keep_alive=self._request_headers['Connection']=='keep-alive'
                self._sock = self._socket_pool.get_socket(self._split_url["host"], self._split_url["port"], self._proxy_info)
            else:
                if not 'Connection' in self._request_headers:
                    self._request_headers['Connection']='close'
                self._sock = _connect_socket(self._split_url["host"], self._split_url["port"], self._proxy_info)
            self._sock.settimeout(_TRANSFER_TIMEOUT)                
            arhead = []
            arhead.append(self._method)
            arhead.append(' ')
            arhead.append(self._split_url["path"])
            arhead.append(' ')
            arhead.append('HTTP/1.1')
            for k in self._request_headers:
                arhead.append('\r\n')
                arhead.append(k)
                arhead.append(': ')
                arhead.append(self._request_headers[k])
                
            arhead.append('\r\n')
            arhead.append('\r\n')
            self._sock.sendall(utils.str_to_bytes(''.join(arhead)))
            self._connected=True
        except Exception as e:
            self._keep_alive=False
            self.disconnect()
            raise e
    
    def get_request_header(self):
        return self._request_headers
    
    def set_request_length(self, l):
        self._request_headers["Content-Length"] = str(l)
    
    def _load_response(self, asynccheck=False):
        if not self._connected:
            raise Exception("Not connected.")
        if not self._response_loaded:
            if asynccheck==False:
                self._response_loaded=True
            else:
                self._sock.settimeout(0.0)
            firstread=True
            data = utils.bytes_new(0)
            while utils.bytes_to_str(data).find('\r\n\r\n') == -1:
                try:
                    app=self._sock.recv(4*1024)
                except Exception as e:
                    if asynccheck==True:
                        self._sock.settimeout(_TRANSFER_TIMEOUT)
                        return
                    else:
                        self._response_loaded=True
                        raise e
                if asynccheck==True:
                    self._response_loaded=True
                    asynccheck=False
                    self._sock.settimeout(_TRANSFER_TIMEOUT)
                if app is None or len(app)==0:
                    if firstread:
                        return None
                    raise Exception('Close connection')
                firstread=False
                data += app
                if len(data)>128*1024:
                    raise Exception('Head too long.')         
            ar = utils.bytes_to_str(data).split('\r\n\r\n')
            head = ar[0].split('\r\n')
            self._response_tmpbody=data[len(ar[0])+4:]
            self._response_header=HTTPHeaders()
            appconn=""
            appkeepalive=""
            for item in head:
                if self._response_code is None:
                    self._response_code = item.split(' ')[1]
                else:
                    apppos = item.index(':')
                    appk=item[0:apppos].strip()
                    self._response_header[appk] = item[apppos+1:].strip()
                    if appk.lower()=="connection":
                        appconn=self._response_header[appk]
                    if appk.lower()=="keep-alive":
                        appkeepalive=self._response_header[appk]
                    if appk.lower()=="content-length":
                        self._response_length=int(self._response_header[appk])
            if self._keep_alive:
                self._keep_alive=appconn.lower().startswith("keep-alive")
                if self._keep_alive:
                    ar=appkeepalive.split(",")
                    for v in ar:
                        v=v.strip()
                        if v.lower().startswith("timeout="):
                            try:
                                self._keep_alive_timeout=int(v[8:])
                            except:
                                None
                        
    
    def get_response_code(self):
        try:
            self._load_response()
            return self._response_code
        except Exception as e:
            self.disconnect(True)
            raise(e)
    
    def get_response_header(self):
        try:
            self._load_response()
            return self._response_header
        except Exception as e:
            self.disconnect(True)
            raise(e)
    
    def get_response_length(self):
        try:
            self._load_response()
            return self._response_length
        except Exception as e:
            self.disconnect(True)
            raise(e)
    
    def read(self, sz):
        try:
            self._load_response()
            oret=None
            lntmp = len(self._response_tmpbody)
            if lntmp>0: 
                if sz>=lntmp:
                    oret=self._response_tmpbody
                    self._response_tmpbody=utils.bytes_new()
                    return oret
                else:
                    oret=self._response_tmpbody[0:sz]
                    self._response_tmpbody=self._response_tmpbody[sz:]
                    return oret
            oret = self._sock.recv(sz)
            return oret
        except Exception as e:
            self.disconnect(True)
            raise(e)
    
    def write(self, bt):
        try:
            bckeck=False
            if self._check_close_counter is None:
                self._check_close_counter=utils.Counter()
                bckeck=True
            elif self._check_close_counter.is_elapsed(0.25):
                self._check_close_counter.reset()
                bckeck=True        
            if bckeck:
                self._load_response(True)
                if self._response_code is not None:
                    raise Exception("Connection error: " + str(self._response_code))
            self._sock.sendall(bt)
        except Exception as e:
            self.disconnect(True)
            raise(e)
    
    def _detach_socket(self):
        s=self._sock
        self._sock=None
        self._connected=False
        return s
    
    def disconnect(self, force=False):
        if force==True:
            self._keep_alive=False
        if self._connected==True:
            self._connected=False
            if self._sock is not None:
                if self._keep_alive and self._socket_pool is not None:
                    self._socket_pool.release_socket(self._sock, self._split_url["host"], self._split_url["port"], self._proxy_info, self._keep_alive_timeout)
                else:
                    _close_socket(self._sock)
                self._sock=None                
    
class HttpDownload(threading.Thread):
    
    def __init__(self, purl, fdst, hds, opts):
        threading.Thread.__init__(self, name="HttpDownload")
        self._close=False
        self._bcomplete=False
        self._file_dst=fdst
        self._http_request=HttpConnection("GET", purl, hds, opts)        
        if "bandwidth_calculator_send" in opts:
            self._bandwidth_calculator_send=opts["bandwidth_calculator_send"]
        else: 
            self._bandwidth_calculator_send=BandwidthCalculator()
        self._progress=0
        self._length=0
        self._calcbps=BandwidthCalculator()
        self._on_start = None
        self._on_progress = None
        self._on_complete = None
        self._on_except = None
        if "events" in opts:
            events=opts["events"]
            if "on_start" in events:
                self._on_start = events["on_start"]
            if "on_progress" in events:
                self._on_progress = events["on_progress"]
            if "on_complete" in events:
                self._on_complete = events["on_complete"]
            if "on_except" in events:
                self._on_except = events["on_except"]
        
    
    def run(self):
        self._sock = None
        fl = None
        try:
            self.fire_on_start()
            bsz=64*1024
            self._http_request.connect()
            if self._http_request.get_response_code()=="200":
                pb=0
                bl=self._http_request.get_response_length()
                data = utils.bytes_new()
                while not self._close and utils.bytes_to_str(data).find('\r\n\r\n') == -1:
                    app=self._http_request.read(4*1024)
                    if app is None or len(app)==0:
                        raise Exception('Close connection')
                    data += app
                    if len(data)>128*1024:
                        raise Exception('Multipart head too long.')                    
                if not self._close:
                    pb+=len(data)
                    head = utils.bytes_to_str(data)
                    pi = head.index('\r\n\r\n')+4
                    bondary = head[0:head.index('\r\n')]
                    self._progress=0
                    self._length=bl-pi-(2+len(bondary)+2+2)
                    fl=utils.file_open(self._file_dst, 'wb')
                    bts = data[pi:]
                    if len(bts)>self._length-self._progress:
                        bts=bts[0:self._length-self._progress]
                    fl.write(bts)
                    lnbf=len(bts)
                    if lnbf>0:
                        self._progress+=lnbf
                        self._calcbps.add(lnbf)
                        self.fire_on_progress(self._progress,self._length,self._calcbps.get_bps())
                    while not self._close and pb<bl:
                        '''
                        appbsz=self._calcbps.get_buffer_size()
                        if bsz!=appbsz:
                            bsz=appbsz
                            self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, bsz)                            
                        '''
                        if self._progress<self._length:
                            l=bsz
                            if l>self._length-self._progress:
                                l=self._length-self._progress
                            buff=self._http_request.read(l)
                            if buff is None or len(buff)==0:
                                break
                            fl.write(buff)
                            lnbf=len(buff)
                            pb+=lnbf
                            self._progress+=lnbf                            
                            self._calcbps.add(lnbf)
                            self.fire_on_progress(self._progress,self._length,self._calcbps.get_bps())
                        else:
                            buff=self._http_request.read(bsz)
                            if buff is None or len(buff)==0:
                                break
                            lnbf=len(buff)
                            pb+=lnbf                            
                    if self._progress==self._length:
                        self._bcomplete=True
                    else:
                        raise Exception("Transfer incomplete.")
            else:
                raise Exception("Error response code: " + str(self._http_request.get_response_code()))
        except:
            e = utils.get_exception()
            self.fire_except(e)
        finally:
            self._close=True
            self._http_request.disconnect()
            if fl is not None:
                fl.close()
            if self._bcomplete:
                self.fire_complete()
            elif utils.path_exists(self._file_dst):
                utils.path_remove(self._file_dst)
    
    def close(self):
        self._close=True
    
    def is_close(self):
        return self._close
    
    def is_complete(self):
        return self._bcomplete
    
    def fire_on_start(self):        
        if self._on_start is not None:
            self._on_start()
    
    def fire_on_progress(self,p,l,bps):        
        if self._on_progress is not None:
            self._on_progress(p,l,bps)
    
    def fire_complete(self):        
        if self._on_complete is not None:
            self._on_complete()
    
    def fire_except(self,e):
        if self._on_except is not None:
            self._on_except(e)
   
class HttpUpload(threading.Thread):
    
    def __init__(self, purl, fsrc, hds, opts):
        threading.Thread.__init__(self, name="HttpUpload")
        self._close=False
        self._bcomplete=False
        self._file_src=fsrc
        self._length=utils.path_size(self._file_src)
        nm=utils.path_basename(self._file_src)
        mt=mimetypes.guess_type(self._file_src)
        if mt is None or mt[0] is None or not isinstance(mt[0], str):
            hds["Content-Type"] = "application/octet-stream"
        else:
            hds["Content-Type"] = mt[0]
        hds["Content-Disposition"] = "attachment; filename=\"" + nm + "\"; filename*=UTF-8''" + utils.url_parse_quote(nm.encode("utf-8"), safe='')
        hds["Expires"] = "Sat, 26 Jul 1997 05:00:00 GMT"    
        self._http_request=HttpConnection("POST", purl, hds, opts)
        self._http_request.set_request_length(self._length)
        if "bandwidth_calculator_send" in opts:
            self._bandwidth_calculator_send=opts["bandwidth_calculator_send"]
        else: 
            self._bandwidth_calculator_send=BandwidthCalculator()        
        self._calcbps=BandwidthCalculator()
        self._on_start = None
        self._on_progress = None
        self._on_complete = None
        self._on_except = None
        if "events" in opts:
            events=opts["events"]
            if "on_start" in events:
                self._on_start = events["on_start"]
            if "on_progress" in events:
                self._on_progress = events["on_progress"]
            if "on_complete" in events:
                self._on_complete = events["on_complete"]
            if "on_except" in events:
                self._on_except = events["on_except"]
        
    
    def run(self):
        self._sock = None
        fl = None
        try:
            self.fire_on_start()            
            bsz=self._bandwidth_calculator_send.get_buffer_size()
            self._http_request.connect()
            fl = utils.file_open(self._file_src, 'rb')
            while not self._close:
                appbsz=self._bandwidth_calculator_send.get_buffer_size()
                if bsz!=appbsz:
                    bsz=appbsz
                    #self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, bsz)
                bts = fl.read(bsz)
                ln = len(bts)
                if ln==0:
                    self._bcomplete=True
                    break
                self._http_request.write(bts)
                self._bandwidth_calculator_send.add(ln)
                self._calcbps.add(ln)
                self.fire_on_progress(self._calcbps.get_transfered(),self._length,self._calcbps.get_bps())
                
            if self._bcomplete:
                if self._http_request.get_response_code()!="200":
                    self._bcomplete=False
                    raise Exception("Error response code: " + str(self._http_request.get_response_code()))
        except:
            e = utils.get_exception()
            self.fire_except(e)
        finally:
            self._close=True
            self._http_request.disconnect()
            if fl is not None:
                fl.close()
            if self._bcomplete:
                self.fire_complete()
        
    def close(self):
        self._close=True
    
    def is_close(self):
        return self._close
    
    def is_complete(self):
        return self._bcomplete
    
    def fire_on_start(self):        
        if self._on_start is not None:
            self._on_start()
    
    def fire_on_progress(self,p,l,bps):        
        if self._on_progress is not None:
            self._on_progress(p,l,bps)
    
    def fire_complete(self):        
        if self._on_complete is not None:
            self._on_complete()
    
    def fire_except(self,e):
        if self._on_except is not None:
            self._on_except(e)


class WebSocketCheckAlive(threading.Thread):
    
    def __init__(self, wsk, opts):
        threading.Thread.__init__(self, name="WebSocketCheckAlive")
        self.daemon=True
        self._intervall=opts["intervall"]
        self._threshold=opts["threshold"]
        self._websoket=wsk
        self._counter_check=utils.Counter()
        self._counter_send=utils.Counter()
        self._semaphore = threading.Condition()

    def _send_keep_alive(self):
        try:
            if not self._websoket.is_close():
                self._websoket._send_ws_ping()
                #print("WEBSOCKET - PING INVIATO!")                
        except:
            #traceback.print_exc()
            None

    def reset(self):
        self._semaphore.acquire()
        try:
            #print("WEBSOCKET - PING RESET!")
            self._counter_check.reset()
        finally:
            self._semaphore.release()
        
    def run(self):
        #print("Thread alive started: " + str(self._websoket))        
        bfireclose=False
        while not self._websoket.is_shutdown():
            time.sleep(1)
            self._semaphore.acquire()
            try:
                #CHECK
                if self._counter_check.is_elapsed((self._intervall+self._threshold)):
                    #print("Thread alive close: " + str(self._websoket))
                    bfireclose=not self._websoket.is_close()
                    break
                
                #SEND
                if self._counter_send.is_elapsed((self._intervall-self._threshold)):
                    #print("Thread alive send: " + str(self._websoket))
                    self._counter_send.reset()
                    self._send_keep_alive()
                
            finally:
                self._semaphore.release()
        self._websoket.shutdown()
        if bfireclose is True:
            self._websoket.fire_close(True)        
        #print("Thread alive stopped: " + str(self._websoket))

class WebSocketReader(threading.Thread):
    
    def __init__(self, wsk):
        threading.Thread.__init__(self, name="WebSocketReader")
        self.daemon=True
        self._websocket = wsk

    def _read_fully(self, sock, ln):
        data = []
        cnt=0
        while ln > cnt:
            s = sock.recv(ln-cnt)
            if s is None or len(s) == 0:
                return ''
            if self._websocket._tdalive is not None:
                self._websocket._tdalive.reset()
            data.append(s)
            cnt+=len(s)
        return utils.bytes_join(data)
        
    
    def run(self):
        #print("Thread read started: " + str(self._websocket))        
        bfireclose=False
        bconnLost=True
        sock = self._websocket.get_socket()
        try:
            while not self._websocket.is_shutdown():
                data = self._read_fully(sock, 2)
                if len(data) == 0:
                    bfireclose=not self._websocket.is_close()
                    break
                else:
                    lendt=0
                    bt0=utils.bytes_get(data,0)
                    bt1=utils.bytes_get(data,1)                    
                    fin=bt0 >> 7
                    if fin==False:
                        raise Exception("TODO continuation frame.")
                    opcode=bt0 & 0xF
                    if bt1 <= 125:
                        lendt = bt1
                    elif bt1 == 126:
                        data = self._read_fully(sock, 2)
                        if len(data) == 0:
                            bfireclose=not self._websocket.is_close()
                            break
                        lendt=struct.unpack('!H',data)[0]
                    elif bt1 == 127:
                        data = self._read_fully(sock, 4)
                        if len(data) == 0:
                            bfireclose=not self._websocket.is_close()
                            break
                        lendt=struct.unpack('!I',data)[0]
                    #Read data
                    if lendt>0:
                        data = self._read_fully(sock, lendt)
                        if len(data) == 0:
                            bfireclose=not self._websocket.is_close()
                            break
                    elif lendt==0:
                        data=utils.bytes_new()
                    else:
                        bfireclose=not self._websocket.is_close()
                        break
                    if opcode == 1: #TEXT
                        self._websocket.fire_data(WebSocket.DATA_STRING, utils.bytes_to_str(data,"utf8"))
                    elif opcode == 2: #BYTES
                        self._websocket.fire_data(WebSocket.DATA_BYTES, data)
                    elif opcode == 9: #PING
                        #print("SESSION - PONG RICEVUTO!")
                        continue
                    elif opcode == 10: #PONG
                        #print("SESSION - PONG RICEVUTO!")
                        continue
                    elif opcode == 8: #CLOSE
                        bconnLost=False
                        bfireclose=not self._websocket.is_close()
                        break
                    else:
                        continue                    
        except:
            e=utils.get_exception()
            bfireclose=not self._websocket.is_close()
            #traceback.print_exc()
            self._websocket.fire_except(e) 
        self._websocket.shutdown()
        if bfireclose is True:
            self._websocket.fire_close(bconnLost)        
        #print("Thread read stopped: " + str(self._websocket))
        

class WebSocket:
    DATA_STRING = ord('s')
    DATA_BYTES = ord('b') 
    FRAME_SIZE_MAX = BUFFER_SIZE_MAX-10 #10 = WEBSOCKET HEADER

    def __init__(self, surl, hds, opts):        
        self._url=surl        
        if hds is None:
            hds={}
        hds["Connection"] = 'keep-alive, Upgrade'
        hds["Upgrade"] = 'websocket'
        self._websocket_key=utils.bytes_to_str(utils.enc_base64_encode(os.urandom(16)))
        hds["Sec-Websocket-Key"] = self._websocket_key        
        hds["Sec-Websocket-Version"] = '13'
        self._http_request=HttpConnection("GET", surl, hds, opts)
                
        if "bandwidth_calculator_send" in opts:
            self._bandwidth_calculator_send=opts["bandwidth_calculator_send"]
        else:
            self._bandwidth_calculator_send=BandwidthCalculator()
                    
        if "send_keepalive" in opts:
            self._send_keepalive=opts["send_keepalive"]
        else:
            self._send_keepalive=None
                    
        self._close=True
        self._connection_lost=False
        self._shutdown=False
        self._on_data= None
        self._on_close = None
        self._on_except = None
        if "events" in opts:
            events=opts["events"]
            if "on_data" in events:
                self._on_data = events["on_data"]
            if "on_close" in events:
                self._on_close = events["on_close"]
            if "on_except" in events:
                self._on_except = events["on_except"]
        self._lock_status = threading.Lock()
        self._lock_send = threading.Lock()
        self._sock = None
        self._tdread = None
        self._tdalive = None
        #WEBSOCKET DATA
        self._ws_data_b0_bytes = 2 % 128
        self._ws_data_b0_string = 1 % 128
        self._ws_data_b0_continuation = 0 %  128
        self._ws_data_struct_1=struct.Struct("!BB")
        self._ws_data_struct_2=struct.Struct("!BBH")
        self._ws_data_struct_3=struct.Struct("!BBQ")
        #WEBSOCKET PING
        self._ws_ping_b0 = 9 % 128
        self._ws_ping_b0 |= 1 << 7
        self._ws_ping_struct=struct.Struct("!BBI")
        #WEBSOCKET PONG
        self._ws_pong_b0 = 10 % 128
        self._ws_pong_b0 |= 1 << 7
        self._ws_pong_struct=struct.Struct("!BBI")
        #WEBSOCKET CLOSE
        self._ws_close_b0 = 8 % 128
        self._ws_close_b0 |= 1 << 7
        self._ws_close_struct=struct.Struct("!BBI")
        
        self._ws_mask_struct=struct.Struct("!B")
        self._ws_mask_send=False
        self._ws_mask_send_byte=0x80
                
        self._sndbuf=None        
        
            
    def open(self):
                        
        if self._sock is not None:
            raise Exception("Already connect.")
        if self._http_request is None:
            raise Exception("Connection failed.")
        try:            
            self._http_request.connect()
            if self._http_request.get_response_code()=='101':
                
                swk=""
                if "Sec-Websocket-Accept" in self._http_request.get_response_header():
                    swk=self._http_request.get_response_header()["Sec-Websocket-Accept"].upper()
                if swk!=self._calculate_websocket_accept(self._websocket_key).upper():
                    raise Exception("Invalid Sec-Websocket-Accept.")
                
                self._close=False
                self._sock=self._http_request._detach_socket()
                self._http_request=None
                self._sock.settimeout(None)
                
                '''
                self._sndbuf=self._bandwidth_calculator_send.get_buffer_size()
                self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, self._sndbuf)
                '''
                
                #Start checkalive thread
                if self._send_keepalive is not None:
                    self._tdalive = WebSocketCheckAlive(self,self._send_keepalive)
                    self._tdalive.start()
                
                #Start read thread
                self._tdread = WebSocketReader(self)
                self._tdread.start()                                
            else:
                l = self._http_request.get_response_length()
                if l>0:
                    raise Exception(utils.bytes_to_str(self._http_request.read(l),"utf8"))
                else:
                    raise Exception("Server error.")
        except:
            e=utils.get_exception()
            self.shutdown()
            raise e
    
    def _calculate_websocket_accept(self, swk):
        swa = swk+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
        swa = hashlib.sha1(utils.str_to_bytes(swa,"utf-8")).digest()
        swa = utils.bytes_to_str(base64.b64encode(swa))
        return swa
    
    def _mask_send_data(self,appdt,btmask):
        ar=[]
        ln=len(appdt)
        if utils.is_py2():
            for i in range(ln):
                ar.append(self._ws_mask_struct.pack(ord(appdt[i]) ^ ord(btmask[i % 4])))
        else:
            for i in range(ln):
                ar.append(self._ws_mask_struct.pack(appdt[i] ^ btmask[i % 4]))        
        return utils.bytes_join(ar)
    
    def get_socket(self):
        return self._sock    
   
    def send_bytes(self, data):
        self._send_ws_data(data,self._ws_data_b0_bytes)        
    
    def send_string(self, s):
        self._send_ws_data(utils.str_to_bytes(s,"utf8"),self._ws_data_b0_string)
        
    def fire_data(self, tp, dt):
        if self._on_data is not None:
            self._on_data(tp, dt)        
            
    def fire_close(self,connlost):        
        with self._lock_status:
            self._connection_lost=connlost
            onc=self._on_close
            self._on_data= None
            self._on_close = None
            self._on_except = None
        if onc is not None:
            onc()
    
    def fire_except(self,e):  
        if self._on_except is not None:
            self._on_except(e) 
    
    def _send_ws_data(self,dt,b0):
        if self._sock is None:
            raise Exception('connection closed.')
        self._lock_send.acquire()
        try:
            p=0
            l = len(dt)
            while l>0:
                appln=l
                if appln>WebSocket.FRAME_SIZE_MAX:
                    appln=WebSocket.FRAME_SIZE_MAX
                    appdt=dt[p:p+appln]                                        
                else:
                    if p==0:
                        appdt=dt
                    else:
                        appdt=dt[p:p+appln]
                    b0 |= 1 << 7
                
                ba=bytearray()
                if appln <= 125:
                    ba.extend(self._ws_data_struct_1.pack(b0, appln | self._ws_mask_send_byte))
                elif appln <= 0xFFFF:
                    ba.extend(self._ws_data_struct_2.pack(b0, 126 | self._ws_mask_send_byte, appln))
                else:
                    ba.extend(self._ws_data_struct_3.pack(b0, 127 | self._ws_mask_send_byte, appln))
                if self._ws_mask_send:
                    btmask=utils.bytes_random(4)
                else:
                    btmask=utils.bytes_init(4)
                ba.extend(btmask)
                if self._ws_mask_send:
                    ba.extend(self._mask_send_data(appdt,btmask))
                else:                    
                    ba.extend(appdt)
                utils.socket_sendall(self._sock,ba)
                l-=appln
                p+=appln
                b0 = self._ws_data_b0_continuation
                self._bandwidth_calculator_send.add(appln)
                
                '''
                newbsz=self._bandwidth_calculator_send.get_buffer_size()
                if newbsz!=self._sndbuf:
                    self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, self._sndbuf)
                    print(str(newbsz))
                '''
                
        finally:
            self._lock_send.release()
            
    def _send_ws_close(self):
        if self._sock is None:
            raise Exception('connection closed.')
        self._lock_send.acquire()
        try:
            utils.socket_sendall(self._sock,self._ws_close_struct.pack(self._ws_close_b0, 0 | self._ws_mask_send_byte, 0))            
        finally:
            self._lock_send.release() 
    
    def _send_ws_ping(self):
        if self._sock is None:
            raise Exception('connection closed.')
        self._lock_send.acquire()
        try:
            utils.socket_sendall(self._sock,self._ws_ping_struct.pack(self._ws_ping_b0, 0 | self._ws_mask_send_byte, 0))
        finally:
            self._lock_send.release()
        
    def _send_ws_pong(self):
        if self._sock is None:
            raise Exception('connection closed.')
        self._lock_send.acquire()
        try:
            utils.socket_sendall(self._sock,self._ws_pong_struct.pack(self._ws_pong_b0, 0 | self._ws_mask_send_byte, 0))
        finally:
            self._lock_send.release()

    def is_close(self):
        with self._lock_status:
            bret = self._close
        return bret
    
    def is_connection_lost(self):
        with self._lock_status:
            bret = self._connection_lost
        return bret        
    
    def is_shutdown(self):
        with self._lock_status:
            bret = self._shutdown
        return bret
    
    def close(self):
        bsendclose=False
        try:
            with self._lock_status:
                if not self._close:
                    self._close=True
                    bsendclose=True
                    self._on_data= None
                    self._on_close = None
                    self._on_except = None
                    #print("session send stream close.")
            if bsendclose:
                self._send_ws_close()
                #Wait shutdown
                cnt = utils.Counter()
                while not self.is_shutdown():
                    time.sleep(0.2)
                    if cnt.is_elapsed(10):
                        break
        except:
            None
            
    
    def shutdown(self):
        
        with self._lock_status:
            if self._shutdown:
                return
            self._close=True
            self._shutdown=True
        
        if self._http_request is not None:
            self._http_request.disconnect()
            self._http_request=None        
        
        if self._sock is not None:
            #Close thread alive
            #if (self._tdalive is not None) and (not self._tdalive.is_close()):
            #    self._tdalive.join(5000)
            self._tdalive = None
            
            #Close thread read
            #if (self._tdread is not None) and (not self._tdread.is_close()):
            #    self._tdread.join(5000)
            self._tdread = None
            
            try:                
                self._sock.shutdown(socket.SHUT_RDWR)
            except:
                None
            try:
                self._sock.close()
            except:
                None
            self._sock = None            
            self._proxy_info = None


class WebSocketSimulate(threading.Thread):
    DATA_STRING = ord('s')
    DATA_BYTES = ord('b') 

    def __init__(self, surl, hds, opts):
        threading.Thread.__init__(self, name="WebSocketSimulate")
        self._url=surl
        self._headers=hds
        self._options=opts
        if "bandwidth_calculator_send" in opts:
            self._bandwidth_calculator_send=opts["bandwidth_calculator_send"]
        else: 
            self._bandwidth_calculator_send=BandwidthCalculator()
        self._close=True
        self._connection_lost=False
        self._shutdown=False
        self._on_data= None
        self._on_close = None
        self._on_except = None
        if "events" in opts:
            events=opts["events"]
            if "on_data" in events:
                self._on_data = events["on_data"]
            if "on_close" in events:
                self._on_close = events["on_close"]
            if "on_except" in events:
                self._on_except = events["on_except"]
        self._condition = threading.Condition()
        self._data_list = []
        self._data_list_size = 0
        
                
    def _get_max_send_size(self):
        bps=self._bandwidth_calculator_send.get_bps()
        if bps>0:
            return bps
        else:
            return 56536
    
    def _read_requests(self, bdetroy=False):
        surl=self._url
        if bdetroy:
            surl+="?destroy=true"
        httpreq=HttpConnection("GET", surl, self._headers, self._options)
        httpreq.connect()
        try:
            if httpreq.get_response_code()=="200":
                self._length=httpreq.get_response_length()
                bsz=64*1024
                arreq=[]
                self._progress=0
                while self._progress<self._length:
                    data=httpreq.read(bsz)
                    if data is None or len(data)==0:
                        break
                    arreq.append(data)
                    self._progress+=len(data)            
                if self._progress==self._length:
                    return utils.url_parse_qs(utils.bytes_to_str(utils.bytes_join(arreq)))
                else:
                    raise Exception("Connection closed.")
            else:
                raise Exception("Error response code: " + str(httpreq.get_response_code()))        
        finally:
            httpreq.disconnect()
    
    def _write_responses(self,arresp):
        btreq=utils.str_to_bytes(json.dumps(arresp))
        hds=self._headers.copy()
        hds["Content-Type"] = "application/json; charset=utf-8"
        hds["Content-Length"] = str(len(btreq))
        httpreq=HttpConnection("POST", self._url, hds, self._options)
        httpreq.connect()
        try:
            bsz=-1
            p=0
            l = len(btreq)   
            while l>0:            
                appbsz=self._bandwidth_calculator_send.get_buffer_size()
                if bsz!=appbsz:
                    bsz=appbsz
                    #self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, bsz)
                appln=l
                if appln>bsz:
                    appln=bsz                    
                    appdt=btreq[p:p+appln]
                else:
                    if p==0:
                        appdt=btreq
                    else:
                        appdt=btreq[p:p+appln]            
                httpreq.write(appdt)
                p+=appln
                l-=appln
                self._bandwidth_calculator_send.add(appln)
            if httpreq.get_response_code()!="200":
                raise Exception("Error response code: " + str(httpreq.get_response_code()))
        finally:
            httpreq.disconnect()
        
    def open(self):
        with self._condition:
            self._close=False
        try:
            arreq=self._read_requests()
            if arreq["status"][0]=="ok":            
                self.start()
            else:
                raise Exception("Connection refused.")        
        except Exception as ex:
            with self._condition:
                self._close=True
            raise ex
    
    def run(self):                        
        try:                        
            while not self.is_close():
                                
                #READ REQUEST
                arreq=self._read_requests()
                                
                if "destroy" in arreq and arreq["destroy"][0]=="true":
                    self._write_responses({})
                    break
                
                if self._on_data is not None:
                    cnt = int(arreq["count"][0])
                    for i in range(cnt):
                        tpdata = ord(arreq["type_" + str(i)][0])
                        prprequest = arreq["data_" + str(i)][0]
                        if tpdata==WebSocketSimulate.DATA_BYTES:
                            prprequest=utils.enc_base64_decode(prprequest)
                        self._on_data(tpdata, prprequest)                
                
                #WRITE RESPONSES
                arsend = {}
                with self._condition:
                    if len(self._data_list)==0:
                        appwt=250
                        if "wait" in arreq:
                            appwt=int(arreq["wait"][0])
                        if appwt<=0 or appwt>10000: #Reason KEEP ALIVE
                            appwt=10000 
                        
                        appwt=appwt/1000.0
                        self._condition.wait(appwt)
                    
                    arcnt = 0
                    lensend = 0
                    while len(self._data_list)>0 and lensend<self._get_max_send_size():
                        sdt = self._data_list.pop(0)
                        arsend["type_" + str(arcnt)]=sdt["type"]
                        arsend["data_" + str(arcnt)]=sdt["data"]                        
                        lensend += len(sdt["data"])
                        arcnt+=1                    
                    arsend["count"]=arcnt
                    arsend["otherdata"]=len(self._data_list)>0
                    if lensend>0:
                        self._data_list_size-=lensend
                        self._condition.notify_all()
                
                self._write_responses(arsend)    
                
            if self.is_close():
                self._read_requests(True)
        except:
            e = utils.get_exception()
            if self._on_except is not None:
                self._on_except(e) 
        finally:
            with self._condition:
                bonclose=not self._close
                self._close=True
            if bonclose and self._on_close is not None:
                self._on_close()
            
    def send_bytes(self, data):
        self._send(chr(WebSocketSimulate.DATA_BYTES), utils.bytes_to_str(utils.enc_base64_encode(data)))        
    
    def send_string(self, s):
        self._send(chr(WebSocketSimulate.DATA_STRING), s)        
    
    def _send(self, t, s):
        with self._condition:
            while not self._close and self._data_list_size>self._get_max_send_size():
                self._condition.wait(0.5)
                                
            if not self._close:
                self._data_list.append({"type": t, "data": s})
                self._data_list_size+=len(s)
                self._condition.notify_all()
    
    def is_close(self):
        with self._condition:
            bret = self._close
        return bret
    
    def close(self):
        with self._condition:
            if not self._close:
                self._close=True
                self._on_data= None
                self._on_close = None
                self._on_except = None
                self._condition.notify_all()
