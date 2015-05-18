#include "MainWindow.h"
#include <DFG/DFGLogWidget.h>
#include <Core/Build.h>

#include <QtCore/QTimer>
#include <QtGui/QMenuBar>
#include <QtGui/QMenu>
#include <QtGui/QAction>
#include <QtGui/QFileDialog>
#include <QtGui/QVBoxLayout>
#include <QtCore/QDir>
#include <QtCore/QCoreApplication>

MainWindow::MainWindow( QSettings *settings )
  : m_settings( settings )
{
  setWindowTitle("Fabric Canvas Standalone");

  DFG::DFGGraph::setSettings(m_settings);

  DockOptions dockOpt = dockOptions();
  dockOpt |= AllowNestedDocks;
  dockOpt ^= AllowTabbedDocks;
  setDockOptions(dockOpt);
  m_hasTimeLinePort = false;
  m_viewport = NULL;
  m_timeLine = NULL;
  m_dfgWidget = NULL;
  m_dfgValueEditor = NULL;

  DFG::DFGConfig config;

  // top menu
  QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
  m_newGraphAction = fileMenu->addAction("New Graph");
  m_loadGraphAction = fileMenu->addAction("Load Graph ...");
  m_saveGraphAction = fileMenu->addAction("Save Graph");
  m_saveGraphAction->setEnabled(false);
  m_saveGraphAsAction = fileMenu->addAction("Save Graph As...");
  fileMenu->addSeparator();
  m_quitAction = fileMenu->addAction("Quit");

  QObject::connect(m_newGraphAction, SIGNAL(triggered()), this, SLOT(onNewGraph()));
  QObject::connect(m_loadGraphAction, SIGNAL(triggered()), this, SLOT(onLoadGraph()));
  QObject::connect(m_saveGraphAction, SIGNAL(triggered()), this, SLOT(onSaveGraph()));
  QObject::connect(m_saveGraphAsAction, SIGNAL(triggered()), this, SLOT(onSaveGraphAs()));
  QObject::connect(m_quitAction, SIGNAL(triggered()), this, SLOT(close()));

  QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
  m_undoAction = editMenu->addAction("Undo");
  m_redoAction = editMenu->addAction("Redo");
  editMenu->addSeparator();
  m_cutAction = editMenu->addAction("Cut");
  m_copyAction = editMenu->addAction("Copy");
  m_pasteAction = editMenu->addAction("Paste");

  QObject::connect(m_undoAction, SIGNAL(triggered()), this, SLOT(onUndo()));
  QObject::connect(m_redoAction, SIGNAL(triggered()), this, SLOT(onRedo()));
  QObject::connect(m_copyAction, SIGNAL(triggered()), this, SLOT(onCopy()));
  QObject::connect(m_pasteAction, SIGNAL(triggered()), this, SLOT(onPaste()));

  QMenu *windowMenu = menuBar()->addMenu(tr("&Window"));
  m_logWindowAction = windowMenu->addAction("LogWidget");
  QObject::connect(m_logWindowAction, SIGNAL(triggered()), this, SLOT(onLogWindow()));

  m_slowOperationLabel = new QLabel();

  QLayout *slowOperationLayout = new QVBoxLayout();
  slowOperationLayout->addWidget( m_slowOperationLabel );

  m_slowOperationDialog = new QDialog( this );
  m_slowOperationDialog->setLayout( slowOperationLayout );
  m_slowOperationDialog->setWindowTitle( "Fabric Core" );
  m_slowOperationDialog->setWindowModality( Qt::WindowModal );
  m_slowOperationDialog->setContentsMargins( 10, 10, 10, 10 );
  m_slowOperationDepth = 0;
  m_slowOperationTimer = new QTimer( this );
  connect( m_slowOperationTimer, SIGNAL( timeout() ), m_slowOperationDialog, SLOT( show() ) );

  m_host = NULL;

  m_statusBar = new QStatusBar(this);
  m_fpsLabel = new QLabel( m_statusBar );
  m_statusBar->addPermanentWidget( m_fpsLabel );
  setStatusBar(m_statusBar);
  m_statusBar->show();

  m_fpsTimer.setInterval( 1000 );
  connect( &m_fpsTimer, SIGNAL(timeout()), this, SLOT(updateFPS()) );
  m_fpsTimer.start();

  try
  {
    FabricCore::Client::CreateOptions options;
    memset( &options, 0, sizeof( options ) );
    options.guarded = 1;
    options.optimizationType = FabricCore::ClientOptimizationType_Background;
    options.licenseType = FabricCore::ClientLicenseType_Interactive;
    m_client = FabricCore::Client(
      &DFG::DFGLogWidget::callback,
      0,
      &options
      );
    m_client.loadExtension("Math", "", false);
    m_client.loadExtension("Parameters", "", false);

    m_manager = new ASTWrapper::KLASTManager(&m_client);
    // FE-4147
    // m_manager->loadAllExtensionsFromExtsPath();

    m_host = new DFGWrapper::Host(m_client);

    DFGWrapper::Binding binding = m_host->createBindingToNewGraph();

    DFGWrapper::GraphExecutablePtr subGraph = DFGWrapper::GraphExecutablePtr::StaticCast(binding.getExecutable());

    QGLFormat glFormat;
    glFormat.setDoubleBuffer(true);
    glFormat.setDepth(true);
    glFormat.setAlpha(true);
    glFormat.setSampleBuffers(true);
    glFormat.setSamples(4);

    m_viewport = new Viewports::GLViewportWidget(&m_client, config.defaultWindowColor, glFormat, this);
    setCentralWidget(m_viewport);
    QObject::connect(this, SIGNAL(contentChanged()), m_viewport, SLOT(redraw()));

    // graph view
    m_dfgWidget = new DFG::DFGWidget(NULL, &m_client, m_manager, m_host, binding, subGraph, &m_stack, config);

    QDockWidget *dfgDock = new QDockWidget("Canvas Graph", this);
    dfgDock->setObjectName( "Canvas Graph" );
    dfgDock->setFeatures( QDockWidget::DockWidgetMovable );
    dfgDock->setWidget(m_dfgWidget);
    addDockWidget(Qt::BottomDockWidgetArea, dfgDock, Qt::Vertical);

    // timeline
    QDockWidget *timeLineDock = new QDockWidget("TimeLine", this);
    timeLineDock->setObjectName( "TimeLine" );
    timeLineDock->setFeatures( QDockWidget::DockWidgetMovable );
    m_timeLine = new Viewports::TimeLineWidget(timeLineDock);
    m_timeLine->setTimeRange(1, 50);
    timeLineDock->setWidget(m_timeLine);
    addDockWidget(Qt::BottomDockWidgetArea, timeLineDock, Qt::Vertical);

    // preset library
    QDockWidget *treeDock = new QDockWidget("Presets", this);
    treeDock->setObjectName( "Presets" );
    treeDock->setFeatures( QDockWidget::DockWidgetMovable );
    m_treeWidget = new DFG::PresetTreeWidget(treeDock, m_host);
    treeDock->setWidget(m_treeWidget);
    addDockWidget(Qt::LeftDockWidgetArea, treeDock);

    QObject::connect(m_dfgWidget, SIGNAL(newPresetSaved(QString)), m_treeWidget, SLOT(refresh()));

    // value editor
    QDockWidget *valueDock = new QDockWidget("Values", this);
    valueDock->setObjectName( "Values" );
    valueDock->setFeatures( QDockWidget::DockWidgetMovable );
    m_dfgValueEditor = new DFG::DFGValueEditor(valueDock, m_dfgWidget->getUIController(), config);
    valueDock->setWidget(m_dfgValueEditor);
    addDockWidget(Qt::RightDockWidgetArea, valueDock);

    QObject::connect(m_dfgValueEditor, SIGNAL(valueChanged(ValueItem*)), this, SLOT(onValueChanged()));
    QObject::connect(m_dfgWidget->getUIController(), SIGNAL(structureChanged()), this, SLOT(onStructureChanged()));
    QObject::connect(m_timeLine, SIGNAL(frameChanged(int)), this, SLOT(onFrameChanged(int)));

    QObject::connect(m_dfgWidget, SIGNAL(onGraphSet(FabricUI::GraphView::Graph*)), 
      this, SLOT(onGraphSet(FabricUI::GraphView::Graph*)));

    restoreGeometry( settings->value("mainWindow/geometry").toByteArray() );
    restoreState( settings->value("mainWindow/state").toByteArray() );

    onFrameChanged(m_timeLine->getTime());
    onGraphSet(m_dfgWidget->getUIGraph());
  }
  catch(FabricCore::Exception e)
  {
    printf("Exception: %s\n", e.getDesc_cstr());
    close();
  }
}

void MainWindow::closeEvent( QCloseEvent *event )
{
  m_settings->setValue( "mainWindow/geometry", saveGeometry() );
  m_settings->setValue( "mainWindow/state", saveState() );
  QMainWindow::closeEvent( event );
}

 MainWindow::~MainWindow()
{
  if(m_host)
    delete(m_host);
  if(m_manager)
    delete(m_manager);
}

void MainWindow::hotkeyPressed(Qt::Key key, Qt::KeyboardModifier modifiers, QString hotkey)
{
  if(hotkey == "delete" || hotkey == "delete2")
  {
    std::vector<GraphView::Node *> nodes = m_dfgWidget->getUIGraph()->selectedNodes();
    m_dfgWidget->getUIController()->beginInteraction();
    for(size_t i=0;i<nodes.size();i++)
      m_dfgWidget->getUIController()->removeNode(nodes[i]);
    m_dfgWidget->getUIController()->endInteraction();
  }
  else if(hotkey == "undo")
  {
    onUndo();
  }
  else if(hotkey == "redo")
  {
    onRedo();
  }
  else if(hotkey == "execute")
  {
    onValueChanged();
  }
  else if(hotkey == "frameSelected")
  {
    m_dfgWidget->getUIController()->frameSelectedNodes();
  }
  else if(hotkey == "frameAll")
  {
    m_dfgWidget->getUIController()->frameAllNodes();
  }
  else if(hotkey == "tabSearch")
  {
    QPoint pos = m_dfgWidget->getGraphViewWidget()->lastEventPos();
    pos = m_dfgWidget->getGraphViewWidget()->mapToGlobal(pos);
    m_dfgWidget->getTabSearchWidget()->showForSearch(pos);
  }
  else if(hotkey == "copy")
  {
    m_dfgWidget->getUIController()->copy();
  }
  else if(hotkey == "paste")
  {
    m_dfgWidget->getUIController()->paste();
  }
  else if(hotkey == "new scene")
  {
    onNewGraph();
  }
  else if(hotkey == "open scene")
  {
    onLoadGraph();
  }
  else if(hotkey == "save scene")
  {
    saveGraph(false);
  }
}

void MainWindow::onUndo()
{
  m_stack.undo();
  onValueChanged();
}  

void MainWindow::onRedo()
{
  m_stack.redo();
  onValueChanged();
}  

void MainWindow::onCopy()
{
  m_dfgWidget->getUIController()->copy();
}  

void MainWindow::onPaste()
{
  m_dfgWidget->getUIController()->paste();
}

void MainWindow::onFrameChanged(int frame)
{
  if(!m_hasTimeLinePort)
    return;

  try
  {
    DFGWrapper::Binding binding = m_dfgWidget->getUIController()->getBinding();
    FabricCore::RTVal val = binding.getArgValue("timeline");
    if(!val.isValid())
      binding.setArgValue("timeline", FabricCore::RTVal::ConstructSInt32(m_client, frame));
    else
    {
      std::string typeName = val.getTypeName().getStringCString();
      if(typeName == "SInt32")
        binding.setArgValue("timeline", FabricCore::RTVal::ConstructSInt32(m_client, frame));
      else if(typeName == "UInt32")
        binding.setArgValue("timeline", FabricCore::RTVal::ConstructUInt32(m_client, frame));
      else if(typeName == "Float32")
        binding.setArgValue("timeline", FabricCore::RTVal::ConstructFloat32(m_client, frame));
    }
  }
  catch(FabricCore::Exception e)
  {
    m_dfgWidget->getUIController()->logError(e.getDesc_cstr());
  }

  onValueChanged();
}

void MainWindow::onLogWindow()
{
  QDockWidget *logDock = new QDockWidget("Log", this);
  logDock->setObjectName( "Log" );
  // logDock->setFeatures( QDockWidget::DockWidgetMovable );
  DFG::DFGLogWidget * logWidget = new DFG::DFGLogWidget(logDock);
  logDock->setWidget(logWidget);
  addDockWidget(Qt::TopDockWidgetArea, logDock, Qt::Vertical);
  logDock->setFloating(true);
}

void MainWindow::onValueChanged()
{
  if(m_dfgWidget->getUIController()->execute())
  {
    try
    {
      // DFGWrapper::GraphExecutablePtr graph = m_dfgWidget->getUIController()->getView()->getGraph();
      // DFGWrapper::PortList ports = graph->getPorts();
      // for(size_t i=0;i<ports.size();i++)
      // {
      //   if(ports[i]->getPortType() == FabricCore::DFGPortType_Out)
      //     continue;
      //   FabricCore::RTVal argVal = graph.getWrappedCoreBinding().getArgValue(ports[i]->getName());
      //   m_dfgWidget->getUIController()->log(argVal.getJSON().getStringCString());
      // }
      m_dfgValueEditor->updateOutputs();
      emit contentChanged();
    }
    catch(FabricCore::Exception e)
    {
      m_dfgWidget->getUIController()->logError(e.getDesc_cstr());
    }
  }
}

void MainWindow::onStructureChanged()
{
  if(m_dfgWidget->getUIController()->isViewingRootGraph())
  {
    m_hasTimeLinePort = false;
    try
    {
      DFGWrapper::GraphExecutablePtr graph = m_dfgWidget->getUIController()->getView()->getGraph();
      DFGWrapper::PortList ports = graph->getPorts();
      for(size_t i=0;i<ports.size();i++)
      {
        if(ports[i]->getPortType() == FabricCore::DFGPortType_Out)
          continue;
        std::string portName = ports[i]->getName();
        if(portName != "timeline")
          continue;
        std::string dataType = ports[i]->getResolvedType();
        if(dataType != "Integer" && dataType != "SInt32" && dataType != "UInt32" && dataType != "Float32" && dataType != "Float64")
          continue;
        m_hasTimeLinePort = true;
        break;
      }
    }
    catch(FabricCore::Exception e)
    {
      m_dfgWidget->getUIController()->logError(e.getDesc_cstr());
    }
  }
  onValueChanged();
}

void MainWindow::updateFPS()
{
  if ( !m_viewport )
    return;

  QString caption;
  caption.setNum(m_viewport->fps(), 'f', 2);
  caption += " FPS";
  m_fpsLabel->setText( caption );
}

void MainWindow::onGraphSet(FabricUI::GraphView::Graph * graph)
{
  if(graph)
  {
    m_dfgWidget->getUIGraph()->defineHotkey(Qt::Key_Delete, Qt::NoModifier, "delete");
    m_dfgWidget->getUIGraph()->defineHotkey(Qt::Key_Backspace, Qt::NoModifier, "delete2");
    m_dfgWidget->getUIGraph()->defineHotkey(Qt::Key_Z, Qt::ControlModifier, "undo");
    m_dfgWidget->getUIGraph()->defineHotkey(Qt::Key_Y, Qt::ControlModifier, "redo");
    m_dfgWidget->getUIGraph()->defineHotkey(Qt::Key_F5, Qt::NoModifier, "execute");
    m_dfgWidget->getUIGraph()->defineHotkey(Qt::Key_F, Qt::NoModifier, "frameSelected");
    m_dfgWidget->getUIGraph()->defineHotkey(Qt::Key_A, Qt::NoModifier, "frameAll");
    m_dfgWidget->getUIGraph()->defineHotkey(Qt::Key_Tab, Qt::NoModifier, "tabSearch");
    m_dfgWidget->getUIGraph()->defineHotkey(Qt::Key_C, Qt::ControlModifier, "copy");
    m_dfgWidget->getUIGraph()->defineHotkey(Qt::Key_V, Qt::ControlModifier, "paste");
    m_dfgWidget->getUIGraph()->defineHotkey(Qt::Key_N, Qt::ControlModifier, "new scene");
    m_dfgWidget->getUIGraph()->defineHotkey(Qt::Key_O, Qt::ControlModifier, "open scene");
    m_dfgWidget->getUIGraph()->defineHotkey(Qt::Key_S, Qt::ControlModifier, "save scene");
    QObject::connect(m_dfgWidget->getUIGraph(), SIGNAL(hotkeyPressed(Qt::Key, Qt::KeyboardModifier, QString)), 
      this, SLOT(hotkeyPressed(Qt::Key, Qt::KeyboardModifier, QString)));

    QObject::connect(graph, SIGNAL(hotkeyPressed(Qt::Key, Qt::KeyboardModifier, QString)), 
      this, SLOT(hotkeyPressed(Qt::Key, Qt::KeyboardModifier, QString)));
    QObject::connect(graph, SIGNAL(nodeDoubleClicked(FabricUI::GraphView::Node*)), 
      this, SLOT(onNodeDoubleClicked(FabricUI::GraphView::Node*)));
    QObject::connect(graph, SIGNAL(sidePanelDoubleClicked(FabricUI::GraphView::SidePanel*)), 
      this, SLOT(onSidePanelDoubleClicked(FabricUI::GraphView::SidePanel*)));
  }
}

void MainWindow::onNodeDoubleClicked(FabricUI::GraphView::Node * node)
{
  DFGWrapper::GraphExecutablePtr graph = m_dfgWidget->getUIController()->getView()->getGraph();
  DFGWrapper::NodePtr codeNode = graph->getNode(node->name().toUtf8().constData());
  m_dfgValueEditor->setNode(codeNode);
}

void MainWindow::onSidePanelDoubleClicked(FabricUI::GraphView::SidePanel * panel)
{
  DFG::DFGController * ctrl = m_dfgWidget->getUIController();
  if(ctrl->isViewingRootGraph())
    m_dfgValueEditor->setNode(DFGWrapper::NodePtr());
}

void MainWindow::onNewGraph()
{
  m_lastFileName = "";
  m_saveGraphAction->setEnabled(false);

  try
  {
    DFGWrapper::Binding binding = m_dfgWidget->getUIController()->getBinding();
    binding.flush();

    m_dfgWidget->getUIController()->clearCommands();
    m_dfgWidget->setGraph(m_host, DFGWrapper::Binding(), DFGWrapper::GraphExecutablePtr());
    m_dfgValueEditor->clear();

    m_host->flushUndoRedo();
    delete(m_host);

    m_viewport->clearInlineDrawing();
    m_stack.clear();

    QCoreApplication::processEvents();

    m_hasTimeLinePort = false;

    m_host = new DFGWrapper::Host(m_client);
    binding = m_host->createBindingToNewGraph();
    DFGWrapper::GraphExecutablePtr graph = DFGWrapper::GraphExecutablePtr::StaticCast(binding.getExecutable());

    m_dfgWidget->setGraph(m_host, binding, graph);
    m_treeWidget->setHost(m_host);
    m_dfgValueEditor->onArgsChanged();

    emit contentChanged();
    onStructureChanged();

    m_viewport->update();
  }
  catch(FabricCore::Exception e)
  {
    printf("Exception: %s\n", e.getDesc_cstr());
  }
  
}

void MainWindow::onLoadGraph()
{
  QString lastPresetFolder = m_settings->value("mainWindow/lastPresetFolder").toString();
  QString filePath = QFileDialog::getOpenFileName(this, "Load preset", lastPresetFolder, "DFG Presets (*.dfg.json)");
  if ( filePath.length() )
  {
    QDir dir(filePath);
    dir.cdUp();
    m_settings->setValue( "mainWindow/lastPresetFolder", dir.path() );
    loadGraph( filePath );
  }
  m_saveGraphAction->setEnabled(true);
}

void MainWindow::loadGraph( QString const &filePath )
{
  m_hasTimeLinePort = false;

  try
  {
    DFGWrapper::Binding binding = m_dfgWidget->getUIController()->getBinding();
    binding.flush();

    m_dfgWidget->setGraph(m_host, DFGWrapper::Binding(), DFGWrapper::GraphExecutablePtr());
    m_dfgValueEditor->clear();

    m_host->flushUndoRedo();
    delete(m_host);

    m_viewport->clearInlineDrawing();
    m_stack.clear();

    QCoreApplication::processEvents();

    m_host = new DFGWrapper::Host(m_client);

    FILE * file = fopen(filePath.toUtf8().constData(), "rb");
    if(file)
    {
      fseek( file, 0, SEEK_END );
      int fileSize = ftell( file );
      rewind( file );

      char * buffer = (char*) malloc(fileSize + 1);
      buffer[fileSize] = '\0';

      fread(buffer, 1, fileSize, file);

      fclose(file);

      std::string json = buffer;
      free(buffer);
  
      DFGWrapper::Binding binding = m_host->createBindingFromJSON(json.c_str());
      DFGWrapper::GraphExecutablePtr graph = DFGWrapper::GraphExecutablePtr::StaticCast(binding.getExecutable());
      m_dfgWidget->setGraph(m_host, binding, graph);

      std::vector<std::string> errors = graph->getErrors();
      for(size_t i=0;i<errors.size();i++)
        m_dfgWidget->getUIController()->checkErrors();

      m_treeWidget->setHost(m_host);
      m_dfgWidget->getUIController()->bindUnboundRTVals();
      m_dfgWidget->getUIController()->clearCommands();
      m_dfgWidget->getUIController()->execute();
      m_dfgValueEditor->onArgsChanged();

      QString tl_start = graph->getMetadata("timeline_start");
      QString tl_end = graph->getMetadata("timeline_end");
      QString tl_current = graph->getMetadata("timeline_current");
      if(tl_start.length() > 0 && tl_end.length() > 0)
        m_timeLine->setTimeRange(tl_start.toInt(), tl_end.toInt());
      if(tl_current.length() > 0)
        m_timeLine->updateTime(tl_current.toInt());

      QString camera_mat44 = graph->getMetadata("camera_mat44");
      QString camera_focalDistance = graph->getMetadata("camera_focalDistance");
      if(camera_mat44.length() > 0 && camera_focalDistance.length() > 0)
      {
        try
        {
          FabricCore::RTVal mat44 = FabricCore::ConstructRTValFromJSON(m_client, "Mat44", camera_mat44.toUtf8().constData());
          FabricCore::RTVal focalDistance = FabricCore::ConstructRTValFromJSON(m_client, "Float32", camera_focalDistance.toUtf8().constData());
          FabricCore::RTVal camera = m_viewport->getCamera();
          camera.callMethod("", "setFromMat44", 1, &mat44);
          camera.callMethod("", "setFocalDistance", 1, &focalDistance);
        }
        catch(FabricCore::Exception e)
        {
          printf("Exception: %s\n", e.getDesc_cstr());
        }
        
      }

      emit contentChanged();
      onStructureChanged();

      m_viewport->update();
    }
  }
  catch(FabricCore::Exception e)
  {
    printf("Exception: %s\n", e.getDesc_cstr());
  }

  m_lastFileName = filePath;
  m_saveGraphAction->setEnabled(true);
}

void MainWindow::onSaveGraph()
{
  saveGraph(false);
}

void MainWindow::onSaveGraphAs()
{
  saveGraph(true);
}

void MainWindow::saveGraph(bool saveAs)
{
  QString filePath = m_lastFileName;
  if(filePath.length() == 0 || saveAs)
  {
    QString lastPresetFolder = m_settings->value("mainWindow/lastPresetFolder").toString();
    filePath = QFileDialog::getSaveFileName(this, "Save preset", lastPresetFolder, "DFG Presets (*.dfg.json)");
    if(filePath.length() == 0)
      return;
  }

  QDir dir(filePath);
  dir.cdUp();
  m_settings->setValue( "mainWindow/lastPresetFolder", dir.path() );

  DFGWrapper::Binding binding = m_dfgWidget->getUIController()->getBinding();
  DFGWrapper::GraphExecutablePtr graph = DFGWrapper::GraphExecutablePtr::StaticCast(binding.getExecutable());

  QString num;
  num.setNum(m_timeLine->getRangeStart());
  graph->setMetadata("timeline_start", num.toUtf8().constData(), false);
  num.setNum(m_timeLine->getRangeEnd());
  graph->setMetadata("timeline_end", num.toUtf8().constData(), false);
  num.setNum(m_timeLine->getTime());
  graph->setMetadata("timeline_current", num.toUtf8().constData(), false);

  try
  {
    FabricCore::RTVal camera = m_viewport->getCamera();
    FabricCore::RTVal mat44 = camera.callMethod("Mat44", "getMat44", 0, 0);
    FabricCore::RTVal focalDistance = camera.callMethod("Float32", "getFocalDistance", 0, 0);

    if(mat44.isValid() && focalDistance.isValid())
    {
      graph->setMetadata("camera_mat44", mat44.getJSON().getStringCString(), false);
      graph->setMetadata("camera_focalDistance", focalDistance.getJSON().getStringCString(), false);
    }
  }
  catch(FabricCore::Exception e)
  {
    printf("Exception: %s\n", e.getDesc_cstr());
  }

  try
  {
    std::string json = binding.exportJSON();
    FILE * file = fopen(filePath.toUtf8().constData(), "wb");
    if(file)
    {
      fwrite(json.c_str(), json.length(), 1, file);
      fclose(file);
    }
  }
  catch(FabricCore::Exception e)
  {
    printf("Exception: %s\n", e.getDesc_cstr());
  }

  m_lastFileName = filePath;
  m_saveGraphAction->setEnabled(true);
}
