import sys,struct
from pathlib import Path
import argparse
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'gow3_pc/scripts'),str(ROOT/'android')]
parser=argparse.ArgumentParser(description="Validate every Android performance guard and execute the resolution selector from an owned SELF")
parser.add_argument('self',type=Path)
args=parser.parse_args()
from prepare import parse_self
from tlou_patches import PERFORMANCE_RESOLUTIONS,PERFORMANCE_EFFECTS,BASE_PATCHES,BASE_SELF_SHA256
import hashlib
if hashlib.sha256(args.self.read_bytes()).hexdigest()!=BASE_SELF_SHA256:
    parser.error("SELF diferente do perfil validado")
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64
from unicorn.x86_const import UC_X86_REG_RDI,UC_X86_REG_RSI,UC_X86_REG_RSP,UC_X86_REG_RAX,UC_X86_REG_XMM0,UC_X86_REG_XMM1
elf,_,ph,*_=parse_self(args.self.read_bytes())
base=0x400000
for name,changes in {**PERFORMANCE_EFFECTS,**PERFORMANCE_RESOLUTIONS,**BASE_PATCHES}.items():
 for a,expected,_ in changes:
  p=next(p for p in ph if p['type']==1 and p['vaddr']<=a and a+len(expected)//2<=p['vaddr']+p['filesz'])
  actual=elf[p['offset']+a-p['vaddr']:p['offset']+a-p['vaddr']+len(expected)//2]
  assert actual.hex()==expected,(name,hex(a),actual.hex(),expected)
 print(name,'guards verified')
for res,changes in PERFORMANCE_RESOLUTIONS.items():
 uc=Uc(UC_ARCH_X86,UC_MODE_64)
 end=max(p['vaddr']+p['memsz'] for p in ph if p['type']==1)
 uc.mem_map(base,(end+0xfff)&~0xfff)
 for p in ph:
  if p['type']==1:uc.mem_write(base+p['vaddr'],bytes(elf[p['offset']:p['offset']+p['filesz']]))
 for at,_,replacement in changes:
  if replacement:uc.mem_write(base+at,bytes.fromhex(replacement))
 uc.mem_map(0x8000000,0x2000)
 # Stop before the shared tail; preserve the selected profile's packed RAX and floats.
 uc.reg_write(UC_X86_REG_RDI,0x8000000);uc.reg_write(UC_X86_REG_RSI,1)
 uc.reg_write(UC_X86_REG_XMM0,(1<<128)-1);uc.reg_write(UC_X86_REG_XMM1,(1<<128)-1)
 uc.emu_start(base+0x9105fa,base+0x9106ed,count=100)
 data=uc.mem_read(0x8000000,0x100)
 w,h=struct.unpack_from('<II',data,8)
 assert (w,h)=={'720':(1280,720),'540':(960,540),'360':(640,360)}[res]
 packed=uc.reg_read(UC_X86_REG_RAX)
 assert packed==((h<<32)|w)
 from unicorn.x86_const import UC_X86_REG_XMM0,UC_X86_REG_XMM1
 floats=[struct.unpack('<f',struct.pack('<I',uc.reg_read(r)&0xffffffff))[0] for r in [UC_X86_REG_XMM0,UC_X86_REG_XMM1]]
 assert floats==[float(w),float(h)],floats
 assert uc.reg_read(UC_X86_REG_XMM0)>>32==0
 assert uc.reg_read(UC_X86_REG_XMM1)>>32==0
 print(res,'executed:',w,h,'float=',floats)
