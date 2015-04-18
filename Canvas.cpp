#include "MainWindow.h"
#include <Core/Build.h>
#include <FabricCore.h>
#include <Style/FabricStyle.h>

int main(int argc, char *argv[])
{
  // QApplication::setGraphicsSystem("raster");

  QApplication app(argc, argv);
  app.setOrganizationName( Fabric::companyNameNoInc );
  app.setApplicationName( "Fabric Canvas Standalone" );
  app.setApplicationVersion( Fabric::buildFullVersion );
  app.setStyle(new FabricUI::Style::FabricStyle());

  QSettings settings;
  MainWindow mainWin( &settings );
  mainWin.show();
  for ( int i = 1; i < argc; ++i )
    mainWin.loadGraph( argv[i] );
  return app.exec();
}
