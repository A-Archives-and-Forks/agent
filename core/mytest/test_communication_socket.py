# -*- coding: utf-8 -*-
'''
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
'''
import communication
import time

if __name__ == "__main__":    
    
    surl = ""
        
    def on_data(tp, dt):
        print(str(tp) + " " + dt)
        ws.send_string(u"ECHO " + dt)

    def on_close():
        print("######### _on_close ")
    
    def on_except(e):
        print("######### _on_except " + str(e))
    
    communication.set_cacerts_path("");
    
    opts={}    
    opts["events"]={"on_data": on_data, "on_close": on_close, "on_except" : on_except}
    ws = communication.WebSocket(surl, {}, opts)
    ws.open()
        
    time.sleep(10)
    ws.close()
    print("END")