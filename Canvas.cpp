#include "MainWindow.h"
#include <Core/Build.h>
#include <FabricCore.h>

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  app.setOrganizationName( Fabric::companyNameNoInc );
  app.setApplicationName( "Fabric Canvas Standalone" );
  app.setApplicationVersion( Fabric::buildFullVersion );

  QSettings settings;
  MainWindow mainWin( &settings );
  mainWin.show();
  for ( int i = 1; i < argc; ++i )
    mainWin.loadGraph( argv[i] );
  return app.exec();
}
