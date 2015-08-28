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
  void onNodeDoubleClicked(FabricUI::GraphView::Node * node);
  void onSidePanelDoubleClicked(FabricUI::GraphView::SidePanel * panel);
  void hotkeyPressed(Qt::Key, Qt::KeyboardModifier, QString);
  void onNewGraph();
  void onLoadGraph();
  void onSaveGraph();
  void onSaveGraphAs();
  void onCut();
  void onCopy();
  void onPaste();
  void onFrameChanged(int frame);
  void updateFPS();
  void onPortManipulationRequested(QString portName);
  void setBlockCompilations( bool blockCompilations );
  void onFileNameChanged(QString fileName);

signals:
  void contentChanged();

protected:

  void closeEvent( QCloseEvent *event );
  void saveGraph(bool saveAs);

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
  QUndoView *m_qUndoView;
  Viewports::TimeLineWidget * m_timeLine;
  bool m_hasTimeLinePort;
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
  QAction *m_cutAction;
  QAction *m_copyAction;
  QAction *m_pasteAction;
  QAction *m_manipAction;

  QString m_windowTitle;
  QString m_lastFileName;
};
