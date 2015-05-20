#
# Copyright 2010-2015 Fabric Software Inc. All rights reserved.
#

import os, platform, sys

thirdpartyDirs = {
  'FABRIC_DIR': "Should point to Fabric Engine's installation folder.",
  'QT_DIR': "Should point to the root of Qt folder.",
  'FABRIC_UI_DIR': "Should point to the root of FabricUI repository checkout.",
}

buildType = 'Release'

# help debug print
if GetOption('help'):
  print ''
  print 'Fabric Engine Canvas build script.'
  print '-----------------------------------'
  print 'Required environment variables: '
  for thirdpartyDir in thirdpartyDirs:
    print thirdpartyDir + ': ' + thirdpartyDirs[thirdpartyDir]
  print ''
  Exit()

# for windows for now use Visual Studio 2012. 
env = Environment(ENV = os.environ, MSVC_VERSION='12.0')

# find the third party libs
for thirdpartyDir in thirdpartyDirs:
  if not os.environ.has_key(thirdpartyDir):
    raise Exception(thirdpartyDir+' env variable not defined. '+thirdpartyDirs[thirdpartyDir])

env.Append(CPPPATH = [os.path.join(os.environ['FABRIC_UI_DIR'], 'stage', 'include')])
env.Append(CPPPATH = [os.path.join(os.environ['FABRIC_UI_DIR'], 'stage', 'include', 'FabricUI')])
env.Append(LIBPATH = [os.path.join(os.environ['FABRIC_DIR'], 'lib')])
env.Append(LIBPATH = [os.path.join(os.environ['FABRIC_UI_DIR'], 'stage', 'lib')])
env.Append(CPPPATH = [os.path.join(os.environ['FABRIC_DIR'], 'include')])
env.Append(CPPPATH = [os.path.join(os.environ['FABRIC_DIR'], 'include', 'FabricServices')])
env.Append(CPPDEFINES = ['FEC_SHARED'])

# Fabric Engine libraries
env.Append(LIBS = ['FabricCore-2.0'])
if platform.system().lower().startswith('win'):
  env.Append(LIBS = ['FabricServices-MSVC-'+env['MSVC_VERSION']+'-mt'])
else:
  env.Append(LIBS = ['FabricServices'])
env.Append(LIBS = ['FabricSplitSearch'])

qtDir = os.environ['QT_DIR']
qtFlags = {}
qtMOC = None
if platform.system().lower().startswith('win'):
  if buildType == 'Debug':
    suffix = 'd4'
  else:
    suffix = '4'
  qtFlags['CPPPATH'] = [os.path.join(qtDir, 'include')]
  qtFlags['LIBPATH'] = [os.path.join(qtDir, 'lib')]
  qtFlags['LIBS'] = ['QtCore'+suffix, 'QtGui'+suffix, 'QtOpenGL'+suffix]
  qtMOC = os.path.join(qtDir, 'bin', 'moc.exe')
elif platform.system().lower().startswith('dar'):
  qtFlags['CPPPATH'] = ['/usr/local/include']
  qtFlags['FRAMEWORKPATH'] = ['/usr/local/lib']
  qtFlags['FRAMEWORKS'] = ['QtCore', 'QtGui', 'QtOpenGL']
  qtMOC = '/usr/local/bin/moc'
elif platform.system().lower().startswith('lin'):
  qtFlags['CPPDEFINES'] = ['_DEBUG']
  qtFlags['CPPPATH'] = ['/usr/include']
  qtFlags['LIBPATH'] = ['/usr/lib']
  qtFlags['LIBS'] = ['QtGui', 'QtCore', 'QtOpenGL']
  qtMOC = '/usr/bin/moc-qt4'

# ui related libraries
env.MergeFlags(qtFlags)
env.Append(LIBS = ['FabricUI'])

qtMOCBuilder = Builder(
  action = [[qtMOC, '-o', '$TARGET', '$SOURCE']],
  prefix = 'moc_',
  suffix = '.cc',
  src_suffix = '.h',
)
env.Append(BUILDERS = {'QTMOC': qtMOCBuilder})

def GlobQObjectHeaders(env, filter):
  headers = Flatten(env.Glob(filter))
  qobjectHeaders = []
  for header in headers:
    content = open(header.srcnode().abspath, 'rb').read()
    if content.find('Q_OBJECT') > -1:
      qobjectHeaders.append(header)
  return qobjectHeaders
Export('GlobQObjectHeaders')
env.AddMethod(GlobQObjectHeaders)

def GlobQObjectSources(env, filter):
  headers = env.GlobQObjectHeaders(filter)
  sources = []
  for header in headers:
    sources += env.QTMOC(header)
  return sources
Export('GlobQObjectSources')
env.AddMethod(GlobQObjectSources)

# standard libraries
if sys.platform == 'win32':
  env.Append(LIBS = ['user32', 'advapi32', 'gdi32', 'shell32', 'ws2_32', 'Opengl32', 'glu32'])
else:
  env.Append(LIBS = ['X11', 'GLU', 'GL', 'dl', 'pthread'])

if sys.platform == 'win32':
  env.Append(CCFLAGS = ['/MT', '/Od'])

headers = Flatten(Glob('*.h'))  
sources = Flatten(Glob('*.cpp'))
sources += Flatten(env.GlobQObjectSources('*.h'))

canvasFiles = env.Program('canvas', sources)
canvasAlias = env.Alias('canvas', canvasFiles)

env.Default(canvasAlias)

