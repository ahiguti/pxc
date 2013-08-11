#!/usr/bin/python3 -B

import sys
import reg
import re

def dropSpace(s):
  return re.sub('\s+$', "", s)
def splitWord(s):
  m = re.search('\w+$', s)
  ms = ''
  if m:
    ms = m.group()
  return [ dropSpace(s[0 : len(s) - len(ms)]), ms ]
def convertType(s):
  if s == 'void': return 'v'
  elif s == 'GLenum': return 'e'
  elif s == 'GLbitfield': return 'm'
  elif s == 'GLfloat': return 'f'
  elif s == 'GLdouble': return 'd'
  elif s == 'GLboolean': return 'b'
  elif s == 'GLbyte': return 'by'
  elif s == 'GLubyte': return 'uby'
  elif s == 'GLshort': return 's'
  elif s == 'GLushort': return 'us'
  elif s == 'GLint': return 'i'
  elif s == 'GLuint': return 'ui'
  elif s == 'GLuint64': return 'ul'
  elif s == 'GLsizei': return 'szi'
  elif s == 'GLintptr': return 'ip'
  elif s == 'GLsizeiptr': return 'szip'
  elif s == 'GLDEBUGPROC': return 'DP'
  elif s == 'GLsync': return 'sync'
  elif s == 'void *': return '*'
  elif s == 'GLvoid *': return 'v*'
  elif s == 'GLenum *': return 'e*'
  elif s == 'GLfloat *': return 'f*'
  elif s == 'GLdouble *': return 'd*'
  elif s == 'GLboolean *': return 'b*'
  elif s == 'GLchar *': return 'c*'
  elif s == 'GLubyte *': return 'uby*'
  elif s == 'GLshort *': return 's*'
  elif s == 'GLushort *': return 'us*'
  elif s == 'GLint *': return 'i*'
  elif s == 'GLuint *': return 'ui*'
  elif s == 'GLint64 *': return 'l*'
  elif s == 'GLuint64 *': return 'ul*'
  elif s == 'GLsizei *': return 'szi*'
  elif s == 'const void *': return '_*'
  elif s == 'const GLvoid *': return '_v*'
  elif s == 'const GLenum *': return '_e*'
  elif s == 'const GLfloat *': return '_f*'
  elif s == 'const GLdouble *': return '_d*'
  elif s == 'const GLboolean *': return '_b*'
  elif s == 'const GLchar *': return '_c*'
  elif s == 'const GLbyte *': return '_by*'
  elif s == 'const GLubyte *': return '_uby*'
  elif s == 'const GLshort *': return '_s*'
  elif s == 'const GLushort *': return '_us*'
  elif s == 'const GLint *': return '_i*'
  elif s == 'const GLuint *': return '_ui*'
  elif s == 'const GLsizei *': return '_szi*'
  elif s == 'const GLintptr *': return '_ip*'
  elif s == 'const GLsizeiptr *': return '_szip*'
  elif s == 'GLvoid **': return 'v**'
  elif s == 'const GLvoid *const*': return '_v*_*'
  elif s == 'const GLchar *const*': return '_c*_*'
  print("TYPE " + s)
  return s

class PXOutputGenerator(reg.COutputGenerator):
  """ """
  def __init__(self,
               errFile = sys.stderr,
               warnFile = sys.stderr,
               diagFile = sys.stdout):
    self.pxCmdBody = ''
    self.pxEnumBody = ''
    self.pxBitfieldBody = ''
    reg.COutputGenerator.__init__(self, errFile, warnFile, diagFile)
  def genEnum(self, enuminfo, name):
    reg.COutputGenerator.genEnum(self, enuminfo, name)
    vstr = enuminfo.elem.get('value')
    if (len(vstr) >= 10):
      if (self.pxBitfieldBody != ""):
        self.pxBitfieldBody += ",\n"
      self.pxBitfieldBody += "  \"" + name + "\""
    else:
      if (self.pxEnumBody != ""):
        self.pxEnumBody += ",\n"
      self.pxEnumBody += "  \"" + name + "\""
  def genCmd(self, cmdinfo, name):
    reg.COutputGenerator.genCmd(self, cmdinfo, name)
    if (self.pxCmdBody != ''):
      self.pxCmdBody += ",\n"
    self.pxCmdBody += self.makePXDecl(cmdinfo.elem)
  def makePXDecl(self, cmd):
    """ """
    proto = cmd.find('proto')
    params = cmd.findall('param')
    pname = ''
    for elem in proto:
      text = reg.noneStr(elem.text)
      tail = reg.noneStr(elem.tail)
      if (elem.tag == 'name'):
        pname = text
      n = len(params)
      mparam = ''
      if n > 0:
        mparam = ","
        for i in range(0,n):
          pp = splitWord(''.join([t for t in params[i].itertext()]))
          pn = pp[1]
          pt = convertType(pp[0])
          lenattr = params[i].get('len')
          if lenattr is None:
            mparam += "L{\"" + pn + "\",\"" + pt + "\"}"
          else:
            mparam += "L{\"" + pn + "\",\"" + pt + "\",\"" + lenattr + "\"}"
          if (i < n - 1):
            mparam += ','
    ptxt = ''.join([t for t in proto.itertext()])
    pp = splitWord(reg.noneStr(ptxt))
    mstr = "  L{\"" + pname + "\",\"" \
      + convertType(pp[0]) + "\"" + mparam + "}"
    return mstr;
  def beginFile(self, genOpts):
    reg.OutputGenerator.beginFile(self, genOpts)
    s = "namespace GL::api;\npublic import meta;\n" \
      + "private metafunction L meta::list;\n"
    print(s, end='', file=self.outFile)
  def endFile(self):
    reg.OutputGenerator.endFile(self)
  def beginFeature(self, interface, emit):
    self.curFeature = interface.get('name')
    # print("FEATURE=" + self.curFeature + "\n", end='', file=self.outFile)
    reg.COutputGenerator.beginFeature(self, interface, emit)
  def endFeature(self):
    #if (self.typeBody != ''):
    #  print(self.typeBody, end='', file=self.outFile)
    #if (self.enumBody != ''):
    #  print(self.enumBody, end='', file=self.outFile)
    #if (self.genOpts.genFuncPointers and self.cmdPointerBody != ''):
    #  print(self.cmdPointerBody, end='', file=self.outFile)
    #if (self.cmdBody != ''):
    #  print(self.cmdBody, end='', file=self.outFile)
    pre = "public metafunction " + self.curFeature + "_ENUMVAL";
    if (self.pxEnumBody != ''):
      print(pre + " L{\n", end='', file=self.outFile)
      print(self.pxEnumBody, end='', file=self.outFile)
      print("\n};\n", end='', file=self.outFile)
    else:
      print(pre + " L;\n", end='', file=self.outFile)
    pre = "public metafunction " + self.curFeature + "_BFVAL";
    if (self.pxBitfieldBody != ''):
      print(pre + " L{\n", end='', file=self.outFile)
      print(self.pxBitfieldBody, end='', file=self.outFile)
      print("\n};\n", end='', file=self.outFile)
    else:
      print(pre + " L;\n", end='', file=self.outFile)
    pre = "public metafunction " + self.curFeature + "_CMD";
    if (self.pxCmdBody != ''):
      print(pre + " L{\n", end='', file=self.outFile)
      print(self.pxCmdBody, end='', file=self.outFile)
      print("\n};\n", end='', file=self.outFile)
    else:
      print(pre + " L;\n", end='', file=self.outFile)
    self.pxEnumBody = ''
    self.pxBitfieldBody = ''
    self.pxCmdBody = ''
    self.curFeature = ''
    reg.OutputGenerator.endFeature(self)

opt = reg.CGeneratorOptions(
  filename="api.px", \
  genFuncPointers=None, \
  profile='*', \
  apiname='gl', \
  emitversions='.+') #, versions='.+')
gen = PXOutputGenerator(diagFile=None)
x = reg.Registry()
x.setGenerator(gen)
x.loadFile(file="./gl.xml")
x.apiGen(opt)

