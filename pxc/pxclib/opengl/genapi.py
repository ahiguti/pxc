#!/usr/bin/python3 -B

import sys
import reg
import re

typeArr = [
  'void',
  'GLenum',
  'GLbitfield',
  'GLfloat',
  'GLdouble',
  'GLboolean',
  'GLbyte',
  'GLubyte',
  'GLshort',
  'GLushort',
  'GLint', # 10
  'GLuint',
  'GLuint64',
  'GLsizei',
  'GLintptr',
  'GLsizeiptr',
  'GLfixed',
  'GLclampx',
  'GLDEBUGPROC',
  'GLsync',
  'void *', # 20
  'GLvoid *',
  'GLenum *',
  'GLfloat *',
  'GLdouble *',
  'GLboolean *',
  'GLchar *',
  'GLubyte *',
  'GLshort *',
  'GLushort *',
  'GLint *', # 30
  'GLuint *',
  'GLint64 *',
  'GLuint64 *',
  'GLsizei *',
  'GLfixed *',
  'const void *',
  'const GLvoid *',
  'const GLenum *',
  'const GLfloat *',
  'const GLdouble *', # 40
  'const GLboolean *',
  'const GLchar *',
  'const GLbyte *',
  'const GLubyte *',
  'const GLshort *',
  'const GLushort *',
  'const GLint *',
  'const GLuint *',
  'const GLsizei *',
  'const GLintptr *', # 50
  'const GLsizeiptr *',
  'const GLfixed *',
  'void **',
  'GLvoid **',
  'const void *const*',
  'const GLvoid *const*',
  'const GLchar *const*', # 57

  'const GLuint64 *',
  'GLuint64EXT',
  'const GLuint64EXT *',
  'GLuint64EXT *',
  'struct _cl_context *',
  'struct _cl_event *',
  'GLDEBUGPROCARB',
  'GLhandleARB',
  'const GLcharARB **',
  'GLcharARB *',
  'GLhandleARB *',
  'const GLcharARB *',
  'GLsizeiptrARB',
  'GLintptrARB',
  'GLeglImageOES',
  'GLclampf',
]
typeMap = dict([(e, i) for i, e in enumerate(typeArr)])

def dropSpace(s):
  return re.sub('\s+$', "", s)
def splitWord(s):
  m = re.search('\w+$', s)
  ms = ''
  if m:
    ms = m.group()
  return [ dropSpace(s[0 : len(s) - len(ms)]), ms ]
def convertType(s):
  return str(typeMap[s]) if (s in typeMap) else "\"FIXME "+s+"\""

class PXOutputGenerator(reg.COutputGenerator):
  """ """
  def __init__(self,
               apiname,
               errFile = sys.stderr,
               warnFile = sys.stderr,
               diagFile = sys.stdout):
    self.apiname = apiname
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
            mparam += "L{\"" + pn + "\"," + pt + "}"
          else:
            mparam += "L{\"" + pn + "\"," + pt + ",\"" + lenattr + "\"}"
          if (i < n - 1):
            mparam += ','
    ptxt = ''.join([t for t in proto.itertext()])
    pp = splitWord(reg.noneStr(ptxt))
    mstr = "  L{\"" + pname + "\"," \
      + convertType(pp[0]) + mparam + "}"
    return mstr;
  def beginFile(self, genOpts):
    reg.OutputGenerator.beginFile(self, genOpts)
    s = "public threaded namespace opengl::api_" + self.apiname + ";\n" \
      + "public import core::meta;\n" \
      + "private metafunction L core::meta::list;\n"
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

def genApi(apiname, profile):
  opt = reg.CGeneratorOptions(
    filename="api_" + apiname + ".px", \
    genFuncPointers=None, \
    profile=profile, \
    # addExtensions='GL_EXT|GL_ARB', \
    # addExtensions='.+', \
    addExtensions='', \
    apiname=apiname, \
    emitversions='.+') #, versions='.+')
  # gen = PXOutputGenerator(diagFile=None, apiname=apiname)
  gen = PXOutputGenerator(diagFile=sys.stdout, apiname=apiname)
  x = reg.Registry()
  x.setGenerator(gen)
  x.loadFile(file="./gl.xml")
  x.apiGen(opt)

genApi('gl', '*');
genApi('gles1', 'common');
genApi('gles2', 'common');

