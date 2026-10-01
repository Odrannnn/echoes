#!/usr/bin/env python3
"""Propose Trilogy (R3ME01) names for G2ME01 addresses, function by function.

A GameCube REL refers to the DOL by address, its Trilogy RSO twin by name. Both
modules are split into functions (REL: config/G2ME01/rels/<Mod>/symbols.txt; RSO:
a boundary heuristic, the RSO carries no function symbols), each function becomes
the list of DOL symbols it relocates against, and functions are paired across the
two compilers by how many of those symbols they share. Function order differs
between the two builds, so the pairing is unordered. Inside a pair, a position
where the REL has an address and the RSO a name is a vote; accepted names feed the
next round. Output: build/trilogy_name_proposals.json, optionally --tsv <path>
for the accepted ones. These are candidates: see docs/research/trilogy_name_pairing.md.

Needs orig/R3ME01/files/MP2/RSO/Production/ (not in the repo). Run from the root.
"""
import struct,os,re,difflib,collections,json,sys
G='orig/G2ME01/files/RelProd'; W='orig/R3ME01/files/MP2/RSO/Production'
def rso(p):
    d=open(p,'rb').read()
    nsec,so=struct.unpack('>II',d[8:16])
    secs=[struct.unpack('>II',d[so+8*i:so+8*i+8]) for i in range(nsec)]
    io_,isz=struct.unpack('>II',d[0x30:0x38])
    eo,es=struct.unpack('>II',d[0x38:0x40])
    xo,xs,xn=struct.unpack('>3I',d[0x40:0x4c])
    io,isz2,inn=struct.unpack('>3I',d[0x4c:0x58])
    names=[]
    for i in range(io,io+isz2,12):
        n=struct.unpack('>I',d[i:i+4])[0]; e=d.index(b'\0',inn+n); names.append(d[inn+n:e].decode('latin1'))
    ext=[]
    for i in range(eo,eo+es,12):
        off,info,add=struct.unpack('>3I',d[i:i+12]); ext.append((off,info&0xff,names[info>>8]))
    intr=[]
    for i in range(io_,io_+isz,12):
        off,info,add=struct.unpack('>3I',d[i:i+12]); intr.append((off,info&0xff,info>>8,add))
    return d,secs,sorted(ext),sorted(intr)
def text_secs(secs):
    return [(secs[1][0]&~1,secs[1][1])]
def funcs(d,secs,intr):
    starts=set()
    ts=text_secs(secs)
    for off,t,sec,add in intr:
        o=secs[sec][0]
        if sec==1 and t in (10,1): starts.add((o&~1)+add)
    out=[]
    for base,size in ts:
        pc=base;end=base+size;cur=base;maxt=base
        while pc<end:
            w=struct.unpack('>I',d[pc:pc+4])[0]
            op=w>>26
            if op==16 and not w&2:   # bc
                bd=w&0xfffc
                if bd&0x8000: bd-=0x10000
                maxt=max(maxt,pc+bd)
            if op==18 and not w&3:
                li=w&0x3fffffc
                if li&0x2000000: li-=0x4000000
                tg=pc+li
                if cur<=tg<end and tg-pc<0x4000 and tg>pc: maxt=max(maxt,tg)
            term = w==0x4e800020 or w==0x4e800420 or (op==18 and not w&1)
            nxt=pc+4
            if (term and nxt>maxt) or (nxt in starts and nxt>maxt):
                # skip zero padding
                while nxt<end and d[nxt:nxt+4]==b'\0\0\0\0': nxt+=4
                out.append((cur,pc+4-cur)); cur=nxt; maxt=nxt; pc=nxt; continue
            pc=nxt
        if cur<end: out.append((cur,end-cur))
    return out
addr2name={}
for l in open('config/G2ME01/symbols.txt'):
    m=re.match(r'(\S+) = \.?(\w+):0x([0-9A-F]+);',l)
    if m: addr2name[int(m.group(3),16)]=m.group(1)
g2names=set(addr2name.values())
def rel(p):
    d=open(p,'rb').read()
    impoff,impsz=struct.unpack('>II',d[0x28:0x30]); out=[]
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
def cls(t): return 'c' if t==10 else 'd'
mods=[]
for f in sorted(os.listdir(G)):
    n=f[:-4]; w=f'{W}/RSO_{n}.rso'; g=f'config/G2ME01/rels/{n}/symbols.txt'
    if not (os.path.exists(w) and os.path.exists(g)): continue
    gf=[]
    for l in open(g):
        m=re.match(r'\S+ = \.text:0x([0-9A-F]+);.*type:function size:0x([0-9A-F]+)',l)
        if m: gf.append((int(m.group(1),16),int(m.group(2),16)))
    gf.sort(); r=[x for x in rel(f'{G}/{f}') if x[0]==1]
    A=[]
    for o,s in gf:
        seq=[];last=None
        for x in r:
            if o<=x[1]<o+s:
                k=(cls(x[2]),x[3])
                if x[2] in (4,6,5) and last==k: continue   # ha/lo pair -> one token
                seq.append(k); last=k
        A.append((o,s,seq))
    d,secs,ext,intr=rso(w)
    B=[]
    for o,s in funcs(d,secs,intr):
        seq=[];last=None
        for x in ext:
            if o<=x[0]<o+s:
                k=(cls(x[1]),x[2])
                if x[1] in (4,6,5) and last==k: continue
                seq.append(k); last=k
        B.append((o,s,seq))
    mods.append((n,A,B))
known={}   # addr -> name
for a,nm in addr2name.items(): known[a]=nm
accepted={}
accinfo={}
def tokA(k): return (k[0],accepted.get(k[1]) or known.get(k[1],'?%x'%k[1]))
for rnd in range(4):
    votes=collections.defaultdict(collections.Counter); npairs=0
    for n,A,B in mods:
        ta=[[tokA(k) for k in a[2]] for a in A]; tb=[b[2] for b in B]
        ca=[collections.Counter(x) for x in ta]; cb=[collections.Counter(x) for x in tb]
        def sim(i,j):
            la,lb=len(ta[i]),len(tb[j])
            if la==0 or lb==0: return 0.0
            return 2*sum((ca[i]&cb[j]).values())/(la+lb)
        na,nb=len(A),len(B)
        cand=[]
        for i in range(na):
            if len(ta[i])<3: continue
            for j in range(nb):
                if len(tb[j])<3: continue
                s_=sim(i,j)
                if s_>=0.5: cand.append((s_,i,j))
        cand.sort(reverse=True); ua=set(); ub=set()
        besta=collections.defaultdict(list)
        for s_,i,j in cand: besta[i].append(s_)
        for s_,i,j in cand:
            if i in ua or j in ub: continue
            # ambiguous: another candidate for i or j with the same score
            if sum(1 for x in cand if x[0]==s_ and (x[1]==i or x[2]==j))>1: continue
            ua.add(i); ub.add(j); npairs+=1
            sm=difflib.SequenceMatcher(None,ta[i],tb[j],autojunk=False)
            for tag,i1,i2,j1,j2 in sm.get_opcodes():
                if tag=='replace' and i2-i1==j2-j1:
                    for k in range(i2-i1):
                        ka=A[i][2][i1+k]; kb=tb[j][j1+k]
                        if ka[0]==kb[0]: votes[ka[1]][kb[1]]+=1
    res=[]
    for addr,c in votes.items():
        (name,nv),=c.most_common(1); tot=sum(c.values())
        res.append((addr,name,nv,tot))
    byname=collections.defaultdict(set)
    for r in res: byname[r[1]].add(r[0])
    new=0
    for addr,name,nv,tot in res:
        if nv>=2 and nv/tot>=0.9 and len(byname[name])==1 and addr not in accepted and name not in g2names:
            accepted[addr]=name; accinfo[addr]=(nv,tot,rnd); new+=1
    print('round',rnd,'function pairs',npairs,'vote targets',len(res),'new accepted',new,'total',len(accepted),file=sys.stderr)
    if not new: break
# final report
out=[]
for addr,name in accepted.items():
    nv,tot,rnd=accinfo[addr]; cur=addr2name.get(addr,'<none>')
    out.append(dict(addr='0x%08X'%addr,current=cur,proposed=name,votes=nv,total=tot,round=rnd,
                    kind='unnamed' if re.match('fn_|lbl_|<none>',cur) else 'rename'))
for addr,c in votes.items():
    (name,nv),=c.most_common(1); tot=sum(c.values()); cur=addr2name.get(addr,'<none>')
    if cur==name or addr in accepted: continue
    out.append(dict(addr='0x%08X'%addr,current=cur,proposed=name,votes=nv,total=tot,round=-1,
                    kind='conflict' if name in g2names else 'weak'))
out.sort(key=lambda r:(r['kind'],-r['votes'],r['addr']))
os.makedirs('build',exist_ok=True)
json.dump(out,open('build/trilogy_name_proposals.json','w'),indent=1)
if '--tsv' in sys.argv:
    with open(sys.argv[sys.argv.index('--tsv')+1],'w') as f:
        f.write('kind\taddr\tvotes\ttotal\tcurrent\tproposed\n')
        for r in out:
            if r['kind'] in ('unnamed','rename'):
                f.write('%s\t%s\t%d\t%d\t%s\t%s\n'%(r['kind'],r['addr'],r['votes'],r['total'],r['current'],r['proposed']))
print(dict(collections.Counter(r['kind'] for r in out)))
