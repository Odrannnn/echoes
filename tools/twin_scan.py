import json,struct,collections,os,sys
od=json.load(open('objdiff.json')); rep=json.load(open('build/report.json'))
tpath={u['name']:u.get('target_path') for u in od['units']}
def funcs(path):
    e=open(path,'rb').read()
    shoff,=struct.unpack('>I',e[0x20:0x24]); shentsize,shnum,shstr=struct.unpack('>HHH',e[0x2E:0x34])
    S=[struct.unpack('>10I',e[shoff+i*shentsize:shoff+i*shentsize+40]) for i in range(shnum)]
    rel=collections.defaultdict(dict)
    sym=None
    for h in S:
        if h[1]==2: sym=h
        if h[1]==4:
            for o in range(h[4],h[4]+h[5],12):
                off,info,_=struct.unpack('>IIi',e[o:o+12]); rel[h[7]][off]=info&0xff
    if not sym: return {}
    strs=S[sym[6]]; out={}
    for o in range(sym[4],sym[4]+sym[5],16):
        nm,val,size,info,_,shndx=struct.unpack('>IIIBBH',e[o:o+16])
        if info&15!=2 or size<=8 or shndx>=shnum: continue
        s=S[shndx]; b=e[s[4]+val:s[4]+val+size]; r=rel.get(shndx,{})
        ws=[]
        for i in range(0,len(b)-3,4):
            w,=struct.unpack('>I',b[i:i+4]); t=r.get(val+i) or r.get(val+i+2)
            if t in (10,): w&=0xFC000003
            elif t: w&=0xFFFF0000
            ws.append(w)
        n=e[strs[4]+nm:e.index(b'\0',strs[4]+nm)].decode()
        out[n]=(size,hash(tuple(ws)))
    return out
matched=set(); un=[]; miss=0
for u in rep['units']:
    p=tpath.get(u['name'])
    if not p or not os.path.exists(p): miss+=1; continue
    try: F=funcs(p)
    except Exception as ex: miss+=1; continue
    rel=not u['name'].startswith('main/')
    for f in u.get('functions') or []:
        x=F.get(f['name'])
        if not x: continue
        if (f.get('fuzzy_match_percent') or 0)>=100: matched.add(x[1])
        else: un.append((x[1],x[0],rel,u['name'],f['name']))
print('units without object',miss,'unmatched fns >8 bytes seen',len(un))
tw=[x for x in un if x[0] in matched]
print('unmatched with an exact matched twin:',len(tw),'DOL',sum(not x[2] for x in tw),'REL',sum(x[2] for x in tw),'bytes',sum(x[1] for x in tw))
rest=[x for x in un if x[0] not in matched]
g=collections.Counter(x[0] for x in rest)
dup=sum(c-1 for c in g.values() if c>1)
print('remaining:',len(rest),'distinct shapes',len(g),'-> free copies once one is solved',dup)
print('biggest groups',sorted(g.values(),reverse=True)[:10])
for b in (32,64,128,256):
    print('twins with size >',b,sum(1 for x in tw if x[1]>b))
big=collections.Counter(x[3].split('/')[0] for x in tw if x[2]); print(big.most_common(8))
