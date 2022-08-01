import sys
import socket
import  struct
import select
import os

class item:
    def __init__(self, ip, port):
        self.ip = ip    
        self.port = port
        self.sock = 0
class CcompileInfo:
    def __init__(self):
        self.exefile = ''  
        self.arrsrc = []
        self.objdata = []


def readfile(filename):
    temp_list = []
    #print("filename=%s" % filename)
    f = open(filename,'r')
    line = f.readline() 
    while line:
        line = line.strip()
        if len(line) > 0 and line[0]!='#':
                #print("line1=%s len=%d" % (line, len(line)))
                temp_list.append(line)
        line = f.readline()
    f.close()
    return temp_list

def GetCompileInfo(arrdata):
    arrfile = []
    idx=0
    cominfo = CcompileInfo()
    while (idx<len(arrdata)):
        if (arrdata[idx].find("actionset") != -1):
            #print("data0=%d" % len(arrdata))
            if (arrdata[idx].find("actionset1") != -1):
                while (arrdata[idx].find("actionset2") == -1):
                    if (arrdata[idx].find("remote-cc") != -1):
                        cnt = 0
                        
                        arrword=arrdata[idx].split(" ")
                        for word in arrword:
                            #print("word=%s" % word)
                            if (cnt >= 2):
                                #arrfile.append(word)
                                cominfo.arrsrc.append(word)
                                #print("word=%s" % word)
                            cnt=cnt+1
                    idx=idx+1
            elif (arrdata[idx].find("actionset2") != -1):
                while (arrdata[idx].find("actionset3") == -1):
                    if (arrdata[idx].find("remote-cc") != -1):
                        cnt = 0
                        
                        arrword=arrdata[idx].split(" ")
                        for word in arrword:
                            #print("word=%s" % word)
                            if (cnt >= 2):
                                #arrfile.append(word)
                                cominfo.arrsrc.append(word)
                                #print("word=%s" % word)
                            cnt=cnt+1
                    idx=idx+1
            elif (arrdata[idx].find("actionset3") != -1):
                idx=idx+1
                idx=idx+1
                tmpfileinfo = []
                if (arrdata[idx].find("requires") != -1):
                    arrword=arrdata[idx].split(" ")
                    for word in arrword:
                        #print("word=%s" % word)
                        if (cnt >= 1):
                            tmpfileinfo.append(word)
                            #print("word=%s" % word)
                        cnt=cnt+1
                    #for tmp in tmpfileinfo:
                        #print("tmp=%s" % tmp)
                idx=idx-1
                if (arrdata[idx].find("remote-cc") != -1):
                        cnt = 0
                        arrword=arrdata[idx].split(" ")
                        for word in arrword:
                            #print("word=%s" % word)
                            if (cnt >= 2):
                                exist = 0
                                for tmp in tmpfileinfo:
                                    if (word.find(tmp) != -1):
                                        exist = 1
                                        break
                                    #print("tmp=%s" % tmp)
                                if (1 == exist):
                                    cominfo.objdata.append(word)
                                    #print("word=%s" % word)
                                else:
                                    cominfo.exefile=word
                                #arrfile.append(word)
                                #print("word=%s" % word)
                            cnt=cnt+1
                    #idx=idx+1
        else:
            idx=idx+1
    return cominfo

def GetIpInfo(arrdata):
    i=0
    port=""
    arripinfo = []
    for data in arrdata:
        if (data.find("actionset") == -1 
            and data[0]!=9 and data.find("remote-cc") == -1
            and data.find("requires") == -1):
            #print(data)
            tmp=data[data.rfind('=')+1:].strip()
            if (0 == i):
                port = tmp
                #print("port= %s" % tmp)
            else:
                #print("address= %s" % tmp)
                arraddr=tmp.split(" ")
                for addr in arraddr:
                    if (addr.find(":") != -1):
                        ip=addr[0:addr.rfind(':')]
                        portinfo=addr[addr.rfind(':')+1:]
                        #print("ip=%s portinfo=%s" % (ip, portinfo))
                        ipinfo = item(ip, portinfo)
                        arripinfo.append(ipinfo)
                    else:
                        #print("ip=%s port=%s" % (addr, port))
                        ipinfo = item(addr, port)
                        arripinfo.append(ipinfo)
            i+=1
    return arripinfo

if len(sys.argv) < 2:
    print("%s filename" % sys.argv[0])
    exit(0)

arrdata = readfile(sys.argv[1])
arripinfo = GetIpInfo(arrdata)
cominfo=GetCompileInfo(arrdata)
inputs = []
outputs = []               


idx=1
for addr in arripinfo:
    #print("ip1=%s port=%s" % (addr.ip, addr.port))
    s = socket.socket()
    s.connect((addr.ip, int(addr.port)))
    buffer  =  struct.pack( "!ii" ,  1 , idx)
    s.send(buffer)
    arripinfo[idx-1].sock = s
    idx+=1
    inputs.append(s)
    #s.close()

mincost = 10000
minseq = 0
recvhostcnt = 0

while inputs:
    readable, writable, exceptional = select.select(inputs, outputs, inputs)
     # 可读
    for s in readable:
        
        data = s.recv(1024)        # 故意设置的这么小
        #print("have data to recv data=%d" % len(data))
        if (0 == len(data)):
            exit(0)
        if data:            
            if (12 == len(data)):
                id, srvseq, cost = struct.unpack("!iii", data)
                recvhostcnt += 1
                #print("id=%d srvseq=%d cost=%d" % (id, srvseq, cost))
                if (mincost > cost):
                    mincost = cost
                    minseq = srvseq
                    #print("minseq=%d" % (minseq))
                if (recvhostcnt >= len(arripinfo)):
                    recvhostcnt = 0
                    statinfo = os.stat(cominfo.arrsrc[0])
                    #print("size=%d" % statinfo.st_size)
                    buffer  =  struct.pack( "!i24s2i" ,  3, cominfo.arrsrc[0].encode("utf-8"), 0, statinfo.st_size)
                    arripinfo[minseq-1].sock.send(buffer)
            elif (140 == len(data)):     
                id, name, idx,status = struct.unpack("!i128sii", data)
                tmp=[]
                for da in name:
                    #print("%d " % da)
                    if (da == 0):
                        break
                    
                    tmp.append(chr(da))
                realname=''.join(tmp)
                if (0 == status):
                    #print("realname=%s name=[%d] idx=%d status=%d tmp=%s" % (realname, len(realname), idx, status, tmp))
                    if (4 == id):
                       # s1 =realname.strip('ram10CØ') 
                        f = open(realname,'r')
                        while (1):
                            bytes = f.read(1024)	
                            if (len(bytes) <= 0):
                                break
                            s.send(bytes.encode("utf-8"))
                        f.close()
                    elif (5 == id):
                        idx += 1
                        if (idx >= len(cominfo.arrsrc)):

                            arrobj = []
                            comcnt = 0
                            for obj in cominfo.objdata:
                                arrobj.append(obj)
                                if (comcnt != len(cominfo.objdata) - 1):
                                    arrobj.append(';')
                                comcnt+=1
                            
                            sendobj=''.join(arrobj)
                            buffer  =  struct.pack( "!i256s24s" ,  6, sendobj.encode("utf-8"), cominfo.exefile.encode("utf-8"))
                            s.send(buffer)
                            #print("sendobj=%s" % sendobj)    
                        else:
                            statinfo = os.stat(realname)
                            #print("size=%d" % statinfo.st_size)
                            buffer  =  struct.pack( "!i24s2i" ,  3, cominfo.arrsrc[idx].encode("utf-8"), idx, statinfo.st_size)
                            arripinfo[minseq-1].sock.send(buffer)
            elif (36 == len(data)):
                id, status, name,filesize = struct.unpack("!ii24si", data)                
                tmp=[]
                for da in name:
                    if (da == 0):
                        break
                    #print("%d " % da)
                    tmp.append(chr(da))
                realname=''.join(tmp)
                #print("id=%d status=%d realname=%s filesize=%d " % (id, status, realname, filesize))
                os.unlink(realname)
                f = open(realname,'wb')
                size = filesize
                while (size > 0):
                    filedata = s.recv(1024)
                    size -= len(filedata);
                    f.write(filedata);
                f.close()
                exefile = "./" + realname
                chgmod = "chmod 777 " + realname
                os.system(chgmod)
                os.system(exefile)
                exit(0)
#for src in cominfo.arrsrc:
 #   print("src=%s " % src)
#for data1 in cominfo.objdata:
#    print("data=%s " % data1)

#print("exefile=%s " % cominfo.exefile)


