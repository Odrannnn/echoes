"""Propose G2ME01 names from the Trilogy (R3ME01) MP2 modules.

Each GameCube REL has a Trilogy RSO built from the same source. The REL refers to the DOL by address,
the RSO by name; aligning the two relocation streams pairs an address with a name.
Needs orig/R3ME01/files/MP2/RSO/Production. Writes build/trilogy_name_proposals.json.
Proposals are candidates to check, not renames to apply: see docs/research/trilogy_name_pairing.md.
"""
import struct,os,re,difflib,collections,json
G='orig/G2ME01/files/RelProd'; W='orig/R3ME01/files/MP2/RSO/Production'
# G2 symbols: addr -> name
addr2name={}
for l in open('config/G2ME01/symbols.txt'):
    m=re.match(r'(\S+) = \.?(\w+):0x([0-9A-F]+);(.*)',l)
    if m: addr2name[int(m.group(3),16)]=(m.group(1),m.group(2),m.group(4))
def rel(p):
    d=open(p,'rb').read()
    nsec,secoff=struct.unpack('>II',d[0xC:0x14])
    impoff,impsz=struct.unpack('>II',d[0x28:0x30])
    out=[]
    for i in range(impoff,impoff+impsz,8):
        mod,ro=struct.unpack('>II',d[i:i+8])
        if mod!=0: continue
        sec=0;off=0
        while True:
            delta,t,s,add=struct.unpack('>HBBI',d[ro:ro+8]); ro+=8
            if t==203: break
            off+=delta
            if t==202: sec=s;off=0;continue
            if t==201: continue
            out.append((sec,off,t,add))
    out.sort(); return out
def rso(p):
    d=open(p,'rb').read()
    eo,es=struct.unpack('>II',d[0x38:0x40])
    io,isz,inn=struct.unpack('>3I',d[0x4c:0x58])
    names=[]
    for i in range(io,io+isz,12):
        n=struct.unpack('>I',d[i:i+4])[0]; e=d.index(b'\0',inn+n); names.append(d[inn+n:e].decode('latin1'))
    out=[]
    for i in range(eo,eo+es,12):
        off,info,add=struct.unpack('>3I',d[i:i+12])
        out.append((off,info&0xff,names[info>>8],add))
    out.sort(); return out
selnames=set()
votes=collections.defaultdict(collections.Counter)
stats=[]
for f in sorted(os.listdir(G)):
    n=f[:-4]; w=f'{W}/RSO_{n}.rso'
    if not os.path.exists(w): continue
    a=rel(f'{G}/{f}'); b=rso(w)
    selnames|={x[2] for x in b}
    for types in ((10,),(1,),(4,6,5)):
        ta=[x for x in a if x[2] in types]; tb=[x for x in b if x[1] in types]
        ka=[addr2name.get(x[3],('?',))[0] for x in ta]
        kb=[x[2] for x in tb]
        sm=difflib.SequenceMatcher(None,ka,kb,autojunk=False)
        for tag,i1,i2,j1,j2 in sm.get_opcodes():
            if tag=='replace' and i2-i1==j2-j1:
                for k in range(i2-i1):
                    votes[ta[i1+k][3]][kb[j1+k]]+=1
        stats.append((n,types,len(ta),len(tb),round(sm.ratio(),3)))
for s in stats[:9]: print(s)
import statistics
print('mean ratio call',statistics.mean(s[4] for s in stats if s[1]==(10,)))
g2names={v[0] for v in addr2name.values()}
print('sel-imported names',len(selnames),'already in G2 symbols',len(selnames&g2names))
used=collections.Counter()
res=[]
for addr,c in votes.items():
    (name,n),=c.most_common(1); tot=sum(c.values())
    cur=addr2name.get(addr,('<none>','',''))
    if name in g2names: continue   # name already placed elsewhere
    res.append((addr,cur[0],name,n,tot))
byname=collections.defaultdict(list)
for r in res: byname[r[2]].append(r)
good=[r for r in res if r[3]==r[4] and len(byname[r[2]])==1]
strong=[r for r in good if r[3]>=2]
print('proposals',len(res),'unanimous+unique',len(good),'with >=2 votes',len(strong))
print('of which current is fn_/lbl_:',sum(1 for r in good if re.match('fn_|lbl_|<none>',r[1])),' named differently:',sum(1 for r in good if not re.match('fn_|lbl_|<none>',r[1])))
json.dump(sorted(good),open('build/trilogy_name_proposals.json','w'),indent=0)
for r in sorted(good,key=lambda r:-r[3])[:25]: print(hex(r[0]),r[1],'->',r[2],r[3])
print('--- renames of already-named:')
for r in [r for r in good if not re.match('fn_|lbl_|<none>',r[1])][:25]: print(hex(r[0]),r[1],'->',r[2],r[3])
