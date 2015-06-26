#
# Copyright 2010-2015 Fabric Software Inc. All rights reserved.
#

import os
Import(
  'env',
  'stageDir',
  'qtFlags',
  'qtInstalledLibs',
  'qtMOCBuilder',
  'capiSharedFiles',
  'commandsFlags',
  'persistenceFlags',
  'uiFlags',
  'capiSharedLibFlags',
  'astWrapperFlags',
  'legacyBoostFlags',
  'splitSearchFlags',
  'splitSearchFiles',
  'codeCompletionFlags',
  'extsFiles',
  'coreTestExts',
  'coreDFGPresets',
  'extsAdditionalDFGPresets',
  'extsGeneratedDFGPresets',
  'buildOS',
  'buildObject',
  'dfgSamples',
  'allServicesLibFiles'
  )

canvasStandaloneEnv = env.Clone()
canvasStandaloneEnv.Append(BUILDERS = {'QTMOC': qtMOCBuilder})

canvasStandaloneEnv.MergeFlags(commandsFlags)
canvasStandaloneEnv.MergeFlags(persistenceFlags)
canvasStandaloneEnv.MergeFlags(capiSharedLibFlags)
canvasStandaloneEnv.MergeFlags(legacyBoostFlags)
canvasStandaloneEnv.MergeFlags(splitSearchFlags)
canvasStandaloneEnv.MergeFlags(astWrapperFlags)
canvasStandaloneEnv.MergeFlags(codeCompletionFlags)
canvasStandaloneEnv.MergeFlags(uiFlags)
canvasStandaloneEnv.MergeFlags(qtFlags)

cppSources = [
  canvasStandaloneEnv.SubstCoreMacros("Canvas.cpp", "Canvas.cpp.template"),
  canvasStandaloneEnv.File('MainWindow.cpp'),
  canvasStandaloneEnv.QTMOC(canvasStandaloneEnv.Glob('*.h')),
]

canvasStandalone = canvasStandaloneEnv.StageEXE("canvas", [cppSources, buildObject])
  
# install sources
sourceDir = stageDir.Dir('Source').Dir('Apps').Dir('Canvas')
canvasStandalone += canvasStandaloneEnv.Install(sourceDir, cppSources[0:2])
canvasStandalone += canvasStandaloneEnv.Install(sourceDir, canvasStandaloneEnv.Glob('*.h'))
canvasStandalone += canvasStandaloneEnv.Install(sourceDir, canvasStandaloneEnv.File('SConstruct'))

canvasStandaloneEnv.Depends(canvasStandalone, capiSharedFiles)
canvasStandaloneEnv.Depends(canvasStandalone, extsFiles)
canvasStandaloneEnv.Depends(canvasStandalone, coreTestExts)
canvasStandaloneEnv.Depends(canvasStandalone, coreDFGPresets)
canvasStandaloneEnv.Depends(canvasStandalone, extsAdditionalDFGPresets)
canvasStandaloneEnv.Depends(canvasStandalone, extsGeneratedDFGPresets)
canvasStandaloneEnv.Depends(canvasStandalone, splitSearchFiles)
canvasStandaloneEnv.Depends(canvasStandalone, dfgSamples)
canvasStandaloneEnv.Depends(canvasStandalone, qtInstalledLibs)
canvasStandaloneEnv.Depends(canvasStandalone, allServicesLibFiles)
canvasStandaloneEnv.Alias('canvasStandalone', canvasStandalone)
canvasStandaloneEnv.Alias('canvas', canvasStandalone)


Return('canvasStandalone')
