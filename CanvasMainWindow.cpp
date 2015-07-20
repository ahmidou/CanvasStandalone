//
// Copyright 2010-2015 Fabric Software Inc. All rights reserved.
//

#include "CanvasMainWindow.h"

#include <FabricUI/DFG/DFGLogWidget.h>
#include <FabricServices/Persistence/RTValToJSONEncoder.hpp>
#include <FabricServices/Persistence/RTValFromJSONDecoder.hpp>
#include <FabricUI/Licensing/Licensing.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtGui/QFileDialog>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QUndoView>
#include <QtGui/QVBoxLayout>

FabricServices::Persistence::RTValToJSONEncoder sRTValEncoder;
FabricServices::Persistence::RTValFromJSONDecoder sRTValDecoder;

void MainWindow::CoreStatusCallback(
  void *userdata,
  char const *destinationData, uint32_t destinationLength,
  char const *payloadData, uint32_t payloadLength
  )
{
  MainWindow *mainWindow = reinterpret_cast<MainWindow *>( userdata );
  FTL::StrRef destination( destinationData, destinationLength );
  FTL::StrRef payload( payloadData, payloadLength );
  if ( destination == FTL_STR( "licensing" ) )
  {
    try
    {
      FabricUI::HandleLicenseData( mainWindow, mainWindow->m_client, payload );
    }
    catch ( FabricCore::Exception e )
    {
      DFG::DFGLogWidget::log( e.getDesc_cstr() );
    }
  }
}

MainWindowEventFilter::MainWindowEventFilter(MainWindow * window)
: QObject(window)
{
  m_window = window;
}

bool MainWindowEventFilter::eventFilter(QObject* object,QEvent* event)
{
  if (event->type() == QEvent::KeyPress) 
  {
    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(event);

    // forward this to the hotkeyPressed functionality...
    if(keyEvent->key() != Qt::Key_Tab)
    {
      m_window->m_viewport->onKeyPressed(keyEvent);
      if(keyEvent->isAccepted())
        return true;

      m_window->m_dfgWidget->onKeyPressed(keyEvent);
      if(keyEvent->isAccepted())
        return true;
    }  
  }

  return QObject::eventFilter(object, event);
};

MainWindow::MainWindow( QSettings *settings )
  : m_dfguiCommandHandler( &m_qUndoStack )
  , m_settings( settings )
{
  setWindowTitle("Fabric Engine");

  DFG::DFGWidget::setSettings(m_settings);

  DockOptions dockOpt = dockOptions();
  dockOpt |= AllowNestedDocks;
  dockOpt ^= AllowTabbedDocks;
  setDockOptions(dockOpt);
  m_hasTimeLinePort = false;
  m_viewport = NULL;
  m_timeLine = NULL;
  m_dfgWidget = NULL;
  m_dfgValueEditor = NULL;
  m_setGraph = NULL;

  DFG::DFGConfig config;

  // top menu
  QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
  m_newGraphAction = fileMenu->addAction("New Graph (Ctrl-N)");
  m_loadGraphAction = fileMenu->addAction("Load Graph ... (Ctrl-O)");
  m_saveGraphAction = fileMenu->addAction("Save Graph (Ctrl-S)");
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
  editMenu->addAction( m_qUndoStack.createUndoAction( editMenu ) );
  editMenu->addAction( m_qUndoStack.createRedoAction( editMenu ) );
  editMenu->addSeparator();
  m_cutAction = editMenu->addAction("Cut (Ctrl-X)");
  m_copyAction = editMenu->addAction("Copy (Ctrl-C)");
  m_pasteAction = editMenu->addAction("Paste (Ctrl-V)");
  m_manipAction = editMenu->addAction("Toggle Manipulation (Q)");

  QObject::connect(m_copyAction, SIGNAL(triggered()), this, SLOT(onCopy()));
  QObject::connect(m_pasteAction, SIGNAL(triggered()), this, SLOT(onPaste()));

  QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
  QMenu *windowMenu = menuBar()->addMenu(tr("&Window"));

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
    options.rtValToJSONEncoder = &sRTValEncoder;
    options.rtValFromJSONDecoder = &sRTValDecoder;
    m_client = FabricCore::Client(
      &DFG::DFGLogWidget::callback,
      0,
      &options
      );
    m_client.loadExtension("Math", "", false);
    m_client.loadExtension("Parameters", "", false);
    m_client.loadExtension("Util", "", false);
    m_client.setStatusCallback( &MainWindow::CoreStatusCallback, this );

    m_manager = new ASTWrapper::KLASTManager(&m_client);
    // FE-4147
    // m_manager->loadAllExtensionsFromExtsPath();

    // construct the eval context rtval
    m_evalContext = FabricCore::RTVal::Create(m_client, "EvalContext", 0, 0);
    m_evalContext = m_evalContext.callMethod("EvalContext", "getInstance", 0, 0);
    m_evalContext.setMember("host", FabricCore::RTVal::ConstructString(m_client, "Canvas"));
    m_evalContext.setMember("graph", FabricCore::RTVal::ConstructString(m_client, ""));

    m_host = m_client.getDFGHost();

    FabricCore::DFGBinding binding = m_host.createBindingToNewGraph();

    FabricCore::DFGExec graph = binding.getExec();

    QGLFormat glFormat;
    glFormat.setDoubleBuffer(true);
    glFormat.setDepth(true);
    glFormat.setAlpha(true);
    glFormat.setSampleBuffers(true);
    glFormat.setSamples(4);

    m_viewport = new Viewports::GLViewportWidget(&m_client, config.defaultWindowColor, glFormat, this, m_settings);
    setCentralWidget(m_viewport);
    QObject::connect(this, SIGNAL(contentChanged()), m_viewport, SLOT(redraw()));
    QObject::connect(m_viewport, SIGNAL(portManipulationRequested(QString)), this, SLOT(onPortManipulationRequested(QString)));
    QAction *setStageVisibleAction = new QAction( "&Stage", 0 );
    setStageVisibleAction->setCheckable( true );
    setStageVisibleAction->setChecked( m_viewport->isStageVisible() );
    QObject::connect(
      setStageVisibleAction, SIGNAL(toggled(bool)),
      m_viewport, SLOT(setStageVisible(bool))
      );

    QAction *blockCompilationsAction = new QAction( "&Block compilations", 0 );
    blockCompilationsAction->setCheckable( true );
    blockCompilationsAction->setChecked( false );
    QObject::connect(
      blockCompilationsAction, SIGNAL(toggled(bool)),
      this, SLOT(setBlockCompilations(bool))
      );

    // graph view
    m_dfgWidget = new DFG::DFGWidget(
      NULL,
      m_client,
      m_host,
      binding,
      FTL::StrRef(),
      graph,
      m_manager,
      &m_dfguiCommandHandler,
      &m_stack,
      config
      );

    QDockWidget::DockWidgetFeatures dockFeatures =
        QDockWidget::DockWidgetMovable
      | QDockWidget::DockWidgetFloatable
      | QDockWidget::DockWidgetClosable;

    QDockWidget *dfgDock = new QDockWidget("Canvas Graph", this);
    dfgDock->setObjectName( "Canvas Graph" );
    dfgDock->setFeatures( dockFeatures );
    dfgDock->setWidget(m_dfgWidget);
    addDockWidget(Qt::BottomDockWidgetArea, dfgDock, Qt::Vertical);

    // timeline
    m_timeLine = new Viewports::TimeLineWidget();
    m_timeLine->setTimeRange(1, 50);
    QDockWidget *timeLineDock = new QDockWidget("TimeLine", this);
    timeLineDock->setObjectName( "TimeLine" );
    timeLineDock->setFeatures( dockFeatures );
    timeLineDock->setWidget(m_timeLine);
    addDockWidget(Qt::BottomDockWidgetArea, timeLineDock, Qt::Vertical);

    // preset library
    m_treeWidget = new DFG::PresetTreeWidget( m_dfgWidget->getDFGController(), config, true, true, true );
    QDockWidget *treeDock = new QDockWidget("Explorer", this);
    treeDock->setObjectName( "Explorer" );
    treeDock->setFeatures( dockFeatures );
    treeDock->setWidget(m_treeWidget);
    addDockWidget(Qt::LeftDockWidgetArea, treeDock);

    QObject::connect(m_dfgWidget, SIGNAL(newPresetSaved(QString)), m_treeWidget, SLOT(refresh()));

    // value editor
    m_dfgValueEditor =
      new DFG::DFGValueEditor(
        m_dfgWidget->getUIController(),
        config
        );
    QObject::connect(
      m_dfgValueEditor, SIGNAL(valueChanged(ValueItem*)),
      this, SLOT(onValueChanged())
      );
    QDockWidget *dfgValueEditorDockWidget =
      new QDockWidget(
        "Value Editor",
        this
        );
    dfgValueEditorDockWidget->setObjectName( "Values" );
    dfgValueEditorDockWidget->setFeatures( dockFeatures );
    dfgValueEditorDockWidget->setWidget( m_dfgValueEditor );
    addDockWidget( Qt::RightDockWidgetArea, dfgValueEditorDockWidget );

    // log widget
    QWidget *logWidget = new DFG::DFGLogWidget;
    QAction *clearLogAction = new QAction( "&Clear Log Messages", 0 );
    QObject::connect(
      clearLogAction, SIGNAL(triggered()),
      logWidget, SLOT(clear())
      );
    QDockWidget *logDockWidget = new QDockWidget( "Log Messages", this );
    logDockWidget->setObjectName( "Log" );
    logDockWidget->setFeatures( dockFeatures );
    logDockWidget->setWidget( logWidget );
    logDockWidget->hide();
    addDockWidget( Qt::TopDockWidgetArea, logDockWidget, Qt::Vertical );

    // undo widget
    QUndoView *qUndoView = new QUndoView( &m_qUndoStack );
    QAction *undoWindowAction = windowMenu->addAction("UndoWidget");
    QObject::connect(
      undoWindowAction, SIGNAL(triggered()),
      qUndoView, SLOT(show())
      );
    QDockWidget *undoDockWidget = new QDockWidget("History", this);
    undoDockWidget->setObjectName( "History" );
    undoDockWidget->setFeatures( dockFeatures );
    undoDockWidget->setWidget(qUndoView);
    undoDockWidget->hide();
    addDockWidget(Qt::LeftDockWidgetArea, undoDockWidget);

    QObject::connect(m_dfgWidget->getUIController(), SIGNAL(structureChanged()), this, SLOT(onStructureChanged()));
    QObject::connect(m_timeLine, SIGNAL(frameChanged(int)), this, SLOT(onFrameChanged(int)));
    QObject::connect(m_dfgWidget->getUIController(), SIGNAL(variablesChanged()), m_treeWidget, SLOT(refresh()));
    QObject::connect(m_manipAction, SIGNAL(triggered()), m_viewport, SLOT(toggleManipulation()));

    QObject::connect(m_dfgWidget, SIGNAL(onGraphSet(FabricUI::GraphView::Graph*)), 
      this, SLOT(onGraphSet(FabricUI::GraphView::Graph*)));

    restoreGeometry( settings->value("mainWindow/geometry").toByteArray() );
    restoreState( settings->value("mainWindow/state").toByteArray() );

    viewMenu->addAction( setStageVisibleAction );
    viewMenu->addSeparator();
    viewMenu->addAction( clearLogAction );
    viewMenu->addSeparator();
    viewMenu->addAction( blockCompilationsAction );

    windowMenu->addAction( dfgDock->toggleViewAction() );
    windowMenu->addAction( treeDock->toggleViewAction() );
    windowMenu->addAction( dfgValueEditorDockWidget->toggleViewAction() );
    windowMenu->addAction( timeLineDock->toggleViewAction() );
    windowMenu->addSeparator();
    windowMenu->addAction( undoDockWidget->toggleViewAction() );
    windowMenu->addAction( logDockWidget->toggleViewAction() );

    onFrameChanged(m_timeLine->getTime());
    onGraphSet(m_dfgWidget->getUIGraph());
  }
  catch(FabricCore::Exception e)
  {
    throw e;
  }

  installEventFilter(new MainWindowEventFilter(this));
}

void MainWindow::closeEvent( QCloseEvent *event )
{
  m_viewport->setManipulationActive(false);
  m_settings->setValue( "mainWindow/geometry", saveGeometry() );
  m_settings->setValue( "mainWindow/state", saveState() );
  QMainWindow::closeEvent( event );
}

 MainWindow::~MainWindow()
{
  if(m_manager)
    delete(m_manager);
}

void MainWindow::hotkeyPressed(Qt::Key key, Qt::KeyboardModifier modifiers, QString hotkey)
{
  if(hotkey == "delete" || hotkey == "delete2")
  {
    std::vector<GraphView::Node *> nodes = m_dfgWidget->getUIGraph()->selectedNodes();
    m_dfgWidget->getUIController()->gvcDoRemoveNodes(nodes);
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
    m_dfgWidget->getUIController()->cmdPaste();
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
  else if(hotkey == "rename node")
  {
    std::vector<GraphView::Node *> nodes = m_dfgWidget->getUIGraph()->selectedNodes();
    if(nodes.size() > 0)
      m_dfgWidget->onEditNodeTitle(nodes[0]);
  }
  else if(hotkey == "relax nodes")
  {
    m_dfgWidget->getUIController()->relaxNodes();
  }
  else if(hotkey == "toggle manipulation")
  {
    m_viewport->toggleManipulation();
  }
}

void MainWindow::onCopy()
{
  m_dfgWidget->getUIController()->copy();
}  

void MainWindow::onPaste()
{
  m_dfgWidget->getUIController()->cmdPaste();
}

void MainWindow::onFrameChanged(int frame)
{
  try
  {
    m_evalContext.setMember("time", FabricCore::RTVal::ConstructFloat32(m_client, frame));
  }
  catch(FabricCore::Exception e)
  {
    m_dfgWidget->getUIController()->logError(e.getDesc_cstr());
  }

  if(!m_hasTimeLinePort)
    return;

  try
  {
    FabricCore::DFGBinding binding = m_dfgWidget->getUIController()->getBinding();
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

void MainWindow::onPortManipulationRequested(QString portName)
{
  try
  {
    DFG::DFGController * controller = m_dfgWidget->getUIController();
    FabricCore::DFGBinding &binding = controller->getBinding();
    FabricCore::DFGExec exec = binding.getExec();
    FTL::StrRef portResolvedType = exec.getExecPortResolvedType(portName.toUtf8().constData());
    FabricCore::RTVal value = m_viewport->getManipTool()->getLastManipVal();
    if(portResolvedType == "Xfo")
    {
      // pass
    }
    else if(portResolvedType == "Mat44")
      value = value.callMethod("Mat44", "toMat44", 0, 0);
    else if(portResolvedType == "Vec3")
      value = value.maybeGetMember("tr");
    else if(portResolvedType == "Quat")
      value = value.maybeGetMember("ori");
    else
    {
      QString message = "Port '"+portName;
      message += "'to be driven has unsupported type '";
      message += portResolvedType.data();
      message += "'.";
      m_dfgWidget->getUIController()->logError(message.toUtf8().constData());
      return;
    }
    controller->setArg(portName.toUtf8().constData(), value);
    controller->execute();
  }
  catch(FabricCore::Exception e)
  {
    m_dfgWidget->getUIController()->logError(e.getDesc_cstr());
  }
}

void MainWindow::onValueChanged()
{
  if(m_dfgWidget->getUIController()->execute())
  {
    try
    {
      // FabricCore::DFGExec graph = m_dfgWidget->getUIController()->getGraph();
      // DFGWrapper::ExecPortList ports = graph->getPorts();
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
      FabricCore::DFGExec graph =
        m_dfgWidget->getUIController()->getExec();
      unsigned portCount = graph.getExecPortCount();
      for(unsigned i=0;i<portCount;i++)
      {
        if(graph.getExecPortType(i) == FabricCore::DFGPortType_Out)
          continue;
        FTL::StrRef portName = graph.getExecPortName(i);
        if(portName != "timeline")
          continue;
        FTL::StrRef dataType = graph.getExecPortResolvedType(i);
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
  if(graph != m_setGraph)
  {
    GraphView::Graph * graph = m_dfgWidget->getUIGraph();
    graph->defineHotkey(Qt::Key_Delete, Qt::NoModifier, "delete");
    graph->defineHotkey(Qt::Key_Backspace, Qt::NoModifier, "delete2");
    graph->defineHotkey(Qt::Key_Z, Qt::ControlModifier, "undo");
    graph->defineHotkey(Qt::Key_Y, Qt::ControlModifier, "redo");
    graph->defineHotkey(Qt::Key_F5, Qt::NoModifier, "execute");
    graph->defineHotkey(Qt::Key_F, Qt::NoModifier, "frameSelected");
    graph->defineHotkey(Qt::Key_A, Qt::NoModifier, "frameAll");
    graph->defineHotkey(Qt::Key_Tab, Qt::NoModifier, "tabSearch");
    graph->defineHotkey(Qt::Key_C, Qt::ControlModifier, "copy");
    graph->defineHotkey(Qt::Key_V, Qt::ControlModifier, "paste");
    graph->defineHotkey(Qt::Key_N, Qt::ControlModifier, "new scene");
    graph->defineHotkey(Qt::Key_O, Qt::ControlModifier, "open scene");
    graph->defineHotkey(Qt::Key_S, Qt::ControlModifier, "save scene");
    graph->defineHotkey(Qt::Key_F2, Qt::NoModifier, "rename node");
    graph->defineHotkey(Qt::Key_R, Qt::ControlModifier, "relax nodes");
    graph->defineHotkey(Qt::Key_Q, Qt::NoModifier, "toggle manipulation");

    QObject::connect(graph, SIGNAL(hotkeyPressed(Qt::Key, Qt::KeyboardModifier, QString)), 
      this, SLOT(hotkeyPressed(Qt::Key, Qt::KeyboardModifier, QString)));
    QObject::connect(graph, SIGNAL(nodeDoubleClicked(FabricUI::GraphView::Node*)), 
      this, SLOT(onNodeDoubleClicked(FabricUI::GraphView::Node*)));
    QObject::connect(graph, SIGNAL(sidePanelDoubleClicked(FabricUI::GraphView::SidePanel*)), 
      this, SLOT(onSidePanelDoubleClicked(FabricUI::GraphView::SidePanel*)));

    m_setGraph = graph;
  }
}

void MainWindow::onNodeDoubleClicked(
  FabricUI::GraphView::Node *node
  )
{
  if ( node->isBackDropNode() )
    return;

  FabricCore::DFGExec coreDFGGraph =
    m_dfgWidget->getUIController()->getExec();
  m_dfgValueEditor->setNodeName( node->name() );
}

void MainWindow::onSidePanelDoubleClicked(FabricUI::GraphView::SidePanel * panel)
{
  DFG::DFGController * ctrl = m_dfgWidget->getUIController();
  if(ctrl->isViewingRootGraph())
    m_dfgValueEditor->setNodeName( 0 );
}

void MainWindow::onNewGraph()
{
  m_lastFileName = "";
  m_saveGraphAction->setEnabled(false);

  try
  {
    FabricUI::DFG::DFGController *dfgController =
      m_dfgWidget->getUIController();

    FabricCore::DFGBinding binding = dfgController->getBinding();
    binding.flush();

    dfgController->clearCommands();
    m_dfgValueEditor->clear();

    m_host.flushUndoRedo();

    m_viewport->clearInlineDrawing();
    m_stack.clear();

    QCoreApplication::processEvents();

    m_hasTimeLinePort = false;

    binding = m_host.createBindingToNewGraph();
    FabricCore::DFGExec exec = binding.getExec();

    dfgController->setBindingExec( binding, FTL::StrRef(), exec );

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
    FabricUI::DFG::DFGController *dfgController =
      m_dfgWidget->getUIController();

    FabricCore::DFGBinding binding = dfgController->getBinding();
    binding.flush();

    m_dfgValueEditor->clear();

    m_host.flushUndoRedo();

    m_viewport->clearInlineDrawing();
    m_stack.clear();

    QCoreApplication::processEvents();

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
  
      FabricCore::DFGBinding binding =
        m_host.createBindingFromJSON( json.c_str() );
      FabricCore::DFGExec exec = binding.getExec();
      dfgController->setBindingExec( binding, FTL::StrRef(), exec );

      m_dfgWidget->getUIController()->checkErrors();

      m_evalContext.setMember("currentFilePath", FabricCore::RTVal::ConstructString(m_client, filePath.toUtf8().constData()));

      m_dfgWidget->getUIController()->bindUnboundRTVals();
      m_dfgWidget->getUIController()->clearCommands();
      m_dfgWidget->getUIController()->execute();
      m_dfgValueEditor->onArgsChanged();

      QString tl_start = exec.getMetadata("timeline_start");
      QString tl_end = exec.getMetadata("timeline_end");
      QString tl_current = exec.getMetadata("timeline_current");
      if(tl_start.length() > 0 && tl_end.length() > 0)
        m_timeLine->setTimeRange(tl_start.toInt(), tl_end.toInt());
      if(tl_current.length() > 0)
        m_timeLine->updateTime(tl_current.toInt());

      QString camera_mat44 = exec.getMetadata("camera_mat44");
      QString camera_focalDistance = exec.getMetadata("camera_focalDistance");
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
    if(m_lastFileName.length() > 0)
    {
      filePath = m_lastFileName;
      if(filePath.toLower().endsWith(".dfg.json"))
        filePath = filePath.left(filePath.length() - 9);
    }
    else
      filePath = lastPresetFolder;

    filePath = QFileDialog::getSaveFileName(this, "Save preset", filePath, "DFG Presets (*.dfg.json)");
    if(filePath.length() == 0)
      return;
    if(filePath.toLower().endsWith(".dfg.json.dfg.json"))
      filePath = filePath.left(filePath.length() - 9);
  }

  QDir dir(filePath);
  dir.cdUp();
  m_settings->setValue( "mainWindow/lastPresetFolder", dir.path() );

  FabricCore::DFGBinding &binding =
    m_dfgWidget->getUIController()->getBinding();
  FabricCore::DFGExec graph = binding.getExec();

  QString num;
  num.setNum(m_timeLine->getRangeStart());
  graph.setMetadata("timeline_start", num.toUtf8().constData(), false);
  num.setNum(m_timeLine->getRangeEnd());
  graph.setMetadata("timeline_end", num.toUtf8().constData(), false);
  num.setNum(m_timeLine->getTime());
  graph.setMetadata("timeline_current", num.toUtf8().constData(), false);

  try
  {
    FabricCore::RTVal camera = m_viewport->getCamera();
    FabricCore::RTVal mat44 = camera.callMethod("Mat44", "getMat44", 0, 0);
    FabricCore::RTVal focalDistance = camera.callMethod("Float32", "getFocalDistance", 0, 0);

    if(mat44.isValid() && focalDistance.isValid())
    {
      graph.setMetadata("camera_mat44", mat44.getJSON().getStringCString(), false);
      graph.setMetadata("camera_focalDistance", focalDistance.getJSON().getStringCString(), false);
    }
  }
  catch(FabricCore::Exception e)
  {
    printf("Exception: %s\n", e.getDesc_cstr());
  }

  try
  {
    FabricCore::DFGStringResult json = binding.exportJSON();
    char const *jsonData;
    uint32_t jsonSize;
    json.getStringDataAndLength( jsonData, jsonSize );
    FILE * file = fopen(filePath.toUtf8().constData(), "wb");
    if(file)
    {
      fwrite(jsonData, jsonSize, 1, file);
      fclose(file);

      m_evalContext.setMember("currentFilePath", FabricCore::RTVal::ConstructString(m_client, filePath.toUtf8().constData()));
    }
  }
  catch(FabricCore::Exception e)
  {
    printf("Exception: %s\n", e.getDesc_cstr());
  }

  m_lastFileName = filePath;
  m_saveGraphAction->setEnabled(true);
}

void MainWindow::setBlockCompilations( bool blockCompilations )
{
  FabricUI::DFG::DFGController *dfgController =
    m_dfgWidget->getDFGController();
  dfgController->setBlockCompilations( blockCompilations );
}
