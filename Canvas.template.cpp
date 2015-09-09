//
// Copyright 2010-2015 Fabric Software Inc. All rights reserved.
//

#include "CanvasMainWindow.h"
#include <FabricCore.h>
#include <FabricUI/Style/FabricStyle.h>
#include <FTL/CStrRef.h>
#include <FTL/Path.h>

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  app.setOrganizationName( "{{FABRIC_COMPANY_NAME_NO_INC}}" );
  app.setApplicationName( "Fabric Canvas Standalone" );
  app.setApplicationVersion( "{{FABRIC_VERSION_MAJ}}.{{FABRIC_VERSION_MIN}}.{{FABRIC_VERSION_REV}}{{FABRIC_VERSION_SUFFIX}}" );
  app.setStyle(new FabricUI::Style::FabricStyle());

  char *fabricDir = getenv( "FABRIC_DIR" );
  if ( fabricDir )
  {
    std::string logoPath = FTL::PathJoin( fabricDir, "Resources" );
    FTL::PathAppendEntry( logoPath, "fe_logo.png" );
    app.setWindowIcon( QIcon( logoPath.c_str() ) );
  }

  QSettings settings;
  try
  {
    int argi = 1;

    bool unguarded = false;
    if ( argi < argc && FTL::CStrRef(argv[argi]) == FTL_STR("-u") )
    {
      printf("Running core in UNGUARDED mode\n");
      unguarded = true;
      ++argi;
    }

    MainWindow mainWin( &settings, unguarded );
    mainWin.show();

    for ( ; argi < argc; ++argi )
      mainWin.loadGraph( argv[argi] );

    return app.exec();
  }
  catch ( FabricCore::Exception e )
  {
    printf("Error loading Canvas Standalone: %s\n", e.getDesc_cstr());
    return 1;
  }
}
