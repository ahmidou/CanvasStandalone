#
# Copyright 2010-2015 Fabric Software Inc. All rights reserved.
#

import os
Import(
  'env',
  'qtFlags',
  'qtInstalledLibs',
  'qtMOCBuilder',
  'capiSharedFiles',
  'commandsFlags',
  'dfgWrapperFlags',
  'uiFlags',
  'capiSharedLibFlags',
  'astWrapperFlags',
  'legacyBoostFlags',
  'splitSearchFlags',
  'splitSearchFiles',
  'codeCompletionFlags',
  'sceneGraphExtsFiles',
  'coreTestExts',
  'coreDFGPresets',
  'extsDFGPresets',
  'sceneGraphExtsPresets',
  'buildOS',
  'buildObject',
  'dfgSamples',
  )

canvasStandaloneEnv = env.Clone()
canvasStandaloneEnv.Append(BUILDERS = {'QTMOC': qtMOCBuilder})

canvasStandaloneEnv.MergeFlags(commandsFlags)
canvasStandaloneEnv.MergeFlags(capiSharedLibFlags)
canvasStandaloneEnv.MergeFlags(legacyBoostFlags)
canvasStandaloneEnv.MergeFlags(splitSearchFlags)
canvasStandaloneEnv.MergeFlags(dfgWrapperFlags)
canvasStandaloneEnv.MergeFlags(astWrapperFlags)
canvasStandaloneEnv.MergeFlags(codeCompletionFlags)
canvasStandaloneEnv.MergeFlags(uiFlags)
canvasStandaloneEnv.MergeFlags(qtFlags)

cppSources = [
  canvasStandaloneEnv.Glob('*.cpp'),
  canvasStandaloneEnv.QTMOC(canvasStandaloneEnv.Glob('*.h')),
  ]

canvasStandalone = canvasStandaloneEnv.StageEXE("canvas", [cppSources, buildObject])
canvasStandaloneEnv.Depends(canvasStandalone, capiSharedFiles)
canvasStandaloneEnv.Depends(canvasStandalone, sceneGraphExtsFiles)
canvasStandaloneEnv.Depends(canvasStandalone, coreTestExts)
canvasStandaloneEnv.Depends(canvasStandalone, coreDFGPresets)
canvasStandaloneEnv.Depends(canvasStandalone, extsDFGPresets)
canvasStandaloneEnv.Depends(canvasStandalone, sceneGraphExtsPresets)
canvasStandaloneEnv.Depends(canvasStandalone, splitSearchFiles)
canvasStandaloneEnv.Depends(canvasStandalone, dfgSamples)
canvasStandaloneEnv.Depends(canvasStandalone, qtInstalledLibs)
canvasStandaloneEnv.Alias('canvasStandalone', canvasStandalone)
Return('canvasStandalone')
