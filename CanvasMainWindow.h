//
// Copyright 2010-2015 Fabric Software Inc. All rights reserved.
//

#include <QtCore/QSettings>
#include <QtGui/QApplication>
#include <QtGui/QDockWidget>
#include <QtGui/QKeyEvent>
#include <QtGui/QLabel>
#include <QtGui/QMainWindow>
#include <QtGui/QStatusBar>
#include <QtGui/QUndoStack>

#include <FabricUI/DFG/DFGUI.h>
#include <FabricUI/DFG/DFGValueEditor.h>
#include <FabricUI/DFG/DFGUICmdHandler_QUndo.h>
#include <FabricUI/DFG/DFGLogWidget.h>

#include <ASTWrapper/KLASTManager.h>

#include <FabricUI/Viewports/TimeLineWidget.h>
#include <FabricUI/Viewports/GLViewportWidget.h>

#define TimeRange_Default_Frame_In      1
#define TimeRange_Default_Frame_Out     50

using namespace FabricServices;
using namespace FabricUI;

class MainWindow;
class QUndoView;

class MainWindowEventFilter : public QObject
{
public:

  MainWindowEventFilter(MainWindow * window);

  bool eventFilter(QObject* object, QEvent* event);

private:

  MainWindow * m_window;
};

class MainWindow : public DFG::DFGMainWindow
{
  Q_OBJECT

  friend class MainWindowEventFilter;
  
public:

  MainWindow(
    QSettings *settings,
    bool unguarded
    );
  ~MainWindow();

  void loadGraph( QString const &filePath );
  static void CoreStatusCallback( void *userdata, char const *destinationData,
                                  uint32_t destinationLength,
                                  char const *payloadData,
                                  uint32_t payloadLength );

public slots:

  void onDirty();
  void onValueChanged();
  void onStructureChanged();
  void onGraphSet(FabricUI::GraphView::Graph * graph);
  void onNodeInspectRequested(FabricUI::GraphView::Node * node);
  void onNodeEditRequested(FabricUI::GraphView::Node * node);
  void onSidePanelInspectRequested();
  void onHotkeyPressed(Qt::Key, Qt::KeyboardModifier, QString);
  void onNewGraph();
  void onLoadGraph();
  void onSaveGraph();
  void onSaveGraphAs();
  void onFrameChanged(int frame);
  void updateFPS();
  void onPortManipulationRequested(QString portName);
  void setBlockCompilations( bool blockCompilations );
  void onFileNameChanged(QString fileName);
  void enableShortCuts(bool enabled);

private slots:
  void onAdditionalMenuActionsRequested(QString name, QMenu * menu, bool prefix);
  void autosave();

signals:
  void contentChanged();

protected:

  void closeEvent( QCloseEvent *event );
  bool saveGraph(bool saveAs);
  bool checkUnsavedChanged();

  bool performSave(
    FabricCore::DFGBinding &binding,
    QString const &filePath
    );

private:

  QUndoStack m_qUndoStack;
  DFG::DFGUICmdHandler_QUndo m_dfguiCommandHandler;

  QSettings *m_settings;

  FabricCore::Client m_client;
  ASTWrapper::KLASTManager * m_manager;
  FabricCore::DFGHost m_host;
  FabricCore::RTVal m_evalContext;
  DFG::PresetTreeWidget * m_treeWidget;
  DFG::DFGWidget * m_dfgWidget;
  DFG::DFGValueEditor * m_dfgValueEditor;
  FabricUI::GraphView::Graph * m_setGraph;
  Viewports::GLViewportWidget * m_viewport;
  DFG::DFGLogWidget * m_logWidget;
  QUndoView *m_qUndoView;
  Viewports::TimeLineWidget * m_timeLine;
  int m_timelinePortIndex;
  QStatusBar *m_statusBar;
  QTimer m_fpsTimer;
  QLabel *m_fpsLabel;

  QDialog *m_slowOperationDialog;
  QLabel *m_slowOperationLabel;
  uint32_t m_slowOperationDepth;
  QTimer *m_slowOperationTimer;

  QAction *m_newGraphAction;
  QAction *m_loadGraphAction;
  QAction *m_saveGraphAction;
  QAction *m_saveGraphAsAction;
  QAction *m_quitAction;
  QAction *m_manipAction;

  QAction * m_setGridVisibleAction;
  QAction * m_setUsingStageAction;
  QAction * m_resetCameraAction;
  QAction * m_clearLogAction;
  QAction * m_blockCompilationsAction;

  QString m_windowTitle;
  QString m_lastFileName;

  uint32_t m_lastSavedBindingVersion;

  static const uint32_t s_autosaveIntervalSec = 30;
  std::string m_autosaveFilename;
  uint32_t m_lastAutosaveBindingVersion;
};
