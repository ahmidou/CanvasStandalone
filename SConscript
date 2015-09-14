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
  'extraDFGPresets',
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
  canvasStandaloneEnv.SubstCoreMacros("Canvas.cpp", "Canvas.template.cpp"),
  canvasStandaloneEnv.File('CanvasMainWindow.cpp'),
  canvasStandaloneEnv.QTMOC(canvasStandaloneEnv.Glob('*.h')),
]

canvasStandalone = canvasStandaloneEnv.StageEXE("canvas", [cppSources, buildObject])
  
# install sources
sourceDir = stageDir.Dir('Source').Dir('Apps').Dir('Canvas')
canvasStandalone += canvasStandaloneEnv.Install(sourceDir, cppSources[0:2])
canvasStandalone += canvasStandaloneEnv.Install(sourceDir, canvasStandaloneEnv.Glob('*.h'))
canvasStandalone += canvasStandaloneEnv.Install(sourceDir, canvasStandaloneEnv.File('SConstruct'))

canvasStandalone.append(
  canvasStandaloneEnv.Install(
    stageDir.Dir('Resources'),
    [ canvasStandaloneEnv.Dir('#').Dir('Python').Dir('Apps').Dir('Resources').Dir('Images').File('fe_logo.png') ]
    )
  )

canvasStandaloneEnv.Depends(canvasStandalone, capiSharedFiles)
canvasStandaloneEnv.Depends(canvasStandalone, extsFiles)
canvasStandaloneEnv.Depends(canvasStandalone, coreTestExts)
canvasStandaloneEnv.Depends(canvasStandalone, extraDFGPresets)
canvasStandaloneEnv.Depends(canvasStandalone, splitSearchFiles)
canvasStandaloneEnv.Depends(canvasStandalone, dfgSamples)
canvasStandaloneEnv.Depends(canvasStandalone, qtInstalledLibs)
canvasStandaloneEnv.Depends(canvasStandalone, allServicesLibFiles)
canvasStandaloneEnv.Alias('canvasStandalone', canvasStandalone)
canvasStandaloneEnv.Alias('canvas', canvasStandalone)

Return('canvasStandalone')
