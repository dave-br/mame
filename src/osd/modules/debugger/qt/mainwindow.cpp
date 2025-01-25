// license:BSD-3-Clause
// copyright-holders:Andrew Gardner
#include "emu.h"
#include "mainwindow.h"

#include "debugger.h"
#include "debug/debugcon.h"
#include "debug/debugcpu.h"
#include "debug/dvdisasm.h"
#include "debug/points.h"
#include "debug/dvsourcecode.h"

#include "util/xmlfile.h"

#include <QtGui/QCloseEvent>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#else
#include <QtWidgets/QAction>
#endif
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QScrollBar>


namespace osd::debugger::qt {

MainWindow::MainWindow(DebuggerQt &debugger, QWidget *parent) :
	WindowQt(debugger, nullptr),
	m_inputHistory(),
	m_exiting(false)
{
	setGeometry(300, 300, 1000, 600);

	//
	// The main frame and its input and log widgets
	//
	QFrame *mainWindowFrame = new QFrame(this);

	// The input line
	m_inputEdit = new QLineEdit(mainWindowFrame);
	connect(m_inputEdit, &QLineEdit::returnPressed, this, &MainWindow::executeCommandSlot);
	connect(m_inputEdit, &QLineEdit::textEdited, this, &MainWindow::commandEditedSlot);
	m_inputEdit->installEventFilter(this);


	// The log view
	m_consoleView = new DebuggerView(DVT_CONSOLE, m_machine, mainWindowFrame);
	m_consoleView->setFocusPolicy(Qt::NoFocus);
	m_consoleView->setPreferBottom(true);

	QVBoxLayout *vLayout = new QVBoxLayout(mainWindowFrame);
	vLayout->addWidget(m_consoleView);
	vLayout->addWidget(m_inputEdit);
	vLayout->setSpacing(3);
	vLayout->setContentsMargins(4,0,4,2);

	setCentralWidget(mainWindowFrame);

	//
	// Options Menu
	//
	// Create three commands
	m_breakpointToggleAct = new QAction("Toggle Breakpoint at Cursor", this);
	m_breakpointEnableAct = new QAction("Disable Breakpoint at Cursor", this);
	m_runToCursorAct = new QAction("Run to Cursor", this);
	m_breakpointToggleAct->setShortcut(Qt::Key_F9);
	m_breakpointEnableAct->setShortcut(Qt::SHIFT | Qt::Key_F9);
	m_runToCursorAct->setShortcut(Qt::Key_F4);
	connect(m_breakpointToggleAct, &QAction::triggered, this, &MainWindow::toggleBreakpointAtCursor);
	connect(m_breakpointEnableAct, &QAction::triggered, this, &MainWindow::enableBreakpointAtCursor);
	connect(m_runToCursorAct, &QAction::triggered, this, &MainWindow::runToCursor);

	// Right bar options
	QActionGroup *rightBarGroup = new QActionGroup(this);
	rightBarGroup->setObjectName("rightbargroup");
	QAction *rightActRaw = new QAction("Raw Opcodes", this);
	QAction *rightActEncrypted = new QAction("Encrypted Opcodes", this);
	QAction *rightActComments = new QAction("Comments", this);
	rightActRaw->setData(int(DASM_RIGHTCOL_RAW));
	rightActEncrypted->setData(int(DASM_RIGHTCOL_ENCRYPTED));
	rightActComments->setData(int(DASM_RIGHTCOL_COMMENTS));
	rightActRaw->setCheckable(true);
	rightActEncrypted->setCheckable(true);
	rightActComments->setCheckable(true);
	rightActRaw->setActionGroup(rightBarGroup);
	rightActEncrypted->setActionGroup(rightBarGroup);
	rightActComments->setActionGroup(rightBarGroup);
	rightActRaw->setShortcut(QKeySequence("Ctrl+R"));
	rightActEncrypted->setShortcut(QKeySequence("Ctrl+E"));
	rightActComments->setShortcut(QKeySequence("Ctrl+N"));
	rightActRaw->setChecked(true);
	connect(rightBarGroup, &QActionGroup::triggered, this, &MainWindow::rightBarChanged);

	// Source-level debugging vs. disassembly
	QActionGroup *srcdbgGroup = new QActionGroup(this);
	srcdbgGroup->setObjectName("srcdbggroup");
	QAction *srcdbgSource = new QAction("Show Source", this);
	QAction *srcdbgDasm = new QAction("Show Disassembly", this);
	srcdbgSource->setData(int(MENU_SHOW_SOURCE));
	srcdbgDasm->setData(int(MENU_SHOW_DISASM));
	srcdbgSource->setCheckable(true);
	srcdbgDasm->setCheckable(true);
	srcdbgSource->setActionGroup(srcdbgGroup);
	srcdbgDasm->setActionGroup(srcdbgGroup);
	srcdbgSource->setShortcut(QKeySequence("Ctrl+U"));
	srcdbgDasm->setShortcut(QKeySequence("Ctrl+Shift+U"));
	srcdbgDasm->setChecked(true);
	connect(srcdbgGroup, &QActionGroup::triggered, this, &MainWindow::srcdbgBarChanged);

	// Assemble the options menu
	QMenu *optionsMenu = menuBar()->addMenu("&Options");
	optionsMenu->addAction(m_breakpointToggleAct);
	optionsMenu->addAction(m_breakpointEnableAct);
	optionsMenu->addAction(m_runToCursorAct);
	optionsMenu->addSeparator();
	optionsMenu->addActions(rightBarGroup->actions());
	optionsMenu->addSeparator();
	optionsMenu->addActions(srcdbgGroup->actions());

	//
	// Images menu
	//
	image_interface_enumerator imageIterTest(m_machine.root_device());
	if (imageIterTest.first())
		createImagesMenu();

	//
	// Dock window menu
	//
	QMenu *dockMenu = menuBar()->addMenu("Doc&ks");

	setCorner(Qt::TopRightCorner, Qt::TopDockWidgetArea);
	setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);

	// The processor dock
	QDockWidget *cpuDock = new QDockWidget("processor", this);
	cpuDock->setObjectName("cpudock");
	cpuDock->setAllowedAreas(Qt::LeftDockWidgetArea);
	m_procFrame = new ProcessorDockWidget(m_machine, cpuDock);
	cpuDock->setWidget(dynamic_cast<QWidget*>(m_procFrame));

	addDockWidget(Qt::LeftDockWidgetArea, cpuDock);
	dockMenu->addAction(cpuDock->toggleViewAction());

	// The disassembly / source-level debugging dock
	m_dasmDock = new QDockWidget("dasm", this);
	m_dasmDock->setObjectName("dasmdock");
	m_dasmDock->setAllowedAreas(Qt::TopDockWidgetArea);

	// Disassembly frame
	m_dasmFrame = new DasmDockWidget(m_machine, m_dasmDock);
	connect(m_dasmFrame->view(), &DebuggerView::updated, this, &MainWindow::dasmViewUpdated);

	// Source-level debugging frame
	m_srcdbgFrame = new SrcdbgDockWidget(m_machine, m_dasmDock);
	m_srcdbgFrame->setVisible(false);
	connect(m_srcdbgFrame->view(), &DebuggerView::updated, this, &MainWindow::dasmViewUpdated);

	m_dasmDock->setWidget(m_dasmFrame);							// Initially show disassembly view

	addDockWidget(Qt::TopDockWidgetArea, m_dasmDock);
	dockMenu->addAction(m_dasmDock->toggleViewAction());
}


MainWindow::~MainWindow()
{
}


void MainWindow::setProcessor(device_t *processor)
{
	// Cpu swap
	m_procFrame->view()->view()->set_source(*m_procFrame->view()->view()->source_for_device(processor));
	m_dasmFrame->view()->view()->set_source(*m_dasmFrame->view()->view()->source_for_device(processor));

	// Scrollbar refresh - seems I should be able to do in the DebuggerView
	m_dasmFrame->view()->verticalScrollBar()->setValue(m_dasmFrame->view()->view()->visible_position().y);
	m_dasmFrame->view()->verticalScrollBar()->setValue(m_dasmFrame->view()->view()->visible_position().y);

	// Window title
	setWindowTitle(string_format("Debug: %s - %s '%s'", m_machine.system().name, processor->name(), processor->tag()).c_str());
}


void MainWindow::restoreConfiguration(util::xml::data_node const &node)
{
	WindowQt::restoreConfiguration(node);

	debug_view_disasm &dasmview = *m_dasmFrame->view()->view<debug_view_disasm>();

	restoreState(QByteArray::fromPercentEncoding(node.get_attribute_string("qtwindowstate", "")));

	auto const rightbar = node.get_attribute_int(ATTR_WINDOW_DISASSEMBLY_RIGHT_COLUMN, dasmview.right_column());
	QActionGroup *const rightBarGroup = findChild<QActionGroup *>("rightbargroup");
	for (QAction *action : rightBarGroup->actions())
	{
		if (action->data().toInt() == rightbar)
		{
			action->trigger();
			break;
		}
	}

	m_dasmFrame->view()->restoreConfigurationFromNode(node);
	m_inputHistory.restoreConfigurationFromNode(node);
}


void MainWindow::saveConfigurationToNode(util::xml::data_node &node)
{
	WindowQt::saveConfigurationToNode(node);

	node.set_attribute_int(ATTR_WINDOW_TYPE, WINDOW_TYPE_CONSOLE);

	debug_view_disasm &dasmview = *m_dasmFrame->view()->view<debug_view_disasm>();
	node.set_attribute_int(ATTR_WINDOW_DISASSEMBLY_RIGHT_COLUMN, dasmview.right_column());
	node.set_attribute("qtwindowstate", saveState().toPercentEncoding().data());

	m_dasmFrame->view()->saveConfigurationToNode(node);
	m_inputHistory.saveConfigurationToNode(node);
}


// Used to intercept the user clicking 'X' in the upper corner
void MainWindow::closeEvent(QCloseEvent *event)
{
	if (!m_exiting)
	{
		// Don't actually close the window - it will be brought back on user break
		debugActRunAndHide();
		event->ignore();
	}
}


// Used to intercept the user hitting the up arrow in the input widget
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
	// Only filter keypresses
	if (event->type() != QEvent::KeyPress)
		return QObject::eventFilter(obj, event);

	QKeyEvent const &keyEvent = *static_cast<QKeyEvent *>(event);

	// Catch up & down keys
	if (keyEvent.key() == Qt::Key_Escape)
	{
		m_inputEdit->clear();
		m_inputHistory.reset();
		return true;
	}
	else if (keyEvent.key() == Qt::Key_Up)
	{
		QString const *const hist = m_inputHistory.previous(m_inputEdit->text());
		if (hist)
		{
			m_inputEdit->setText(*hist);
			m_inputEdit->setSelection(hist->size(), 0);
		}
		return true;
	}
	else if (keyEvent.key() == Qt::Key_Down)
	{
		QString const *const hist = m_inputHistory.next(m_inputEdit->text());
		if (hist)
		{
			m_inputEdit->setText(*hist);
			m_inputEdit->setSelection(hist->size(), 0);
		}
		return true;
	}
	else if (keyEvent.key() == Qt::Key_Enter)
	{
		executeCommand(false);
		return true;
	}
	else
	{
		return QObject::eventFilter(obj, event);
	}
}


void MainWindow::toggleBreakpointAtCursor(bool changedTo)
{
	debug_view_disasm *const dasmView = m_dasmFrame->view()->view<debug_view_disasm>();
	if (dasmView->cursor_visible() && (m_machine.debugger().console().get_visible_cpu() == dasmView->source()->device()))
	{
		std::optional<offs_t> const address = dasmView->selected_address();
		if (!address.has_value())
		{
			return;
		}
		device_debug *const cpuinfo = dasmView->source()->device()->debug();

		// Find an existing breakpoint at this address
		const debug_breakpoint *bp = cpuinfo->breakpoint_find(address.value());

		// If none exists, add a new one
		std::string command;
		if (!bp)
			command = string_format("bpset 0x%X", address.value());
		else
			command = string_format("bpclear 0x%X", bp->index());
		m_machine.debugger().console().execute_command(command, true);
		m_machine.debug_view().update_all();
		m_machine.debugger().refresh_display();
	}
}


void MainWindow::enableBreakpointAtCursor(bool changedTo)
{
	debug_view_disasm *const dasmView = m_dasmFrame->view()->view<debug_view_disasm>();
	if (dasmView->cursor_visible() && (m_machine.debugger().console().get_visible_cpu() == dasmView->source()->device()))
	{
		std::optional<offs_t> const address = dasmView->selected_address();
		if (!address.has_value())
		{
			return;
		}
		device_debug *const cpuinfo = dasmView->source()->device()->debug();

		// Find an existing breakpoint at this address
		const debug_breakpoint *bp = cpuinfo->breakpoint_find(address.value());

		if (bp)
		{
			int32_t const bpindex = bp->index();
			std::string command = string_format(bp->enabled() ? "bpdisable 0x%X" : "bpenable 0x%X", bpindex);
			m_machine.debugger().console().execute_command(command, true);
			m_machine.debug_view().update_all();
			m_machine.debugger().refresh_display();
		}
	}
}


void MainWindow::runToCursor(bool changedTo)
{
	debug_view_disasm *const dasmView = m_dasmFrame->view()->view<debug_view_disasm>();
	if (dasmView->cursor_visible() && (m_machine.debugger().console().get_visible_cpu() == dasmView->source()->device()))
	{
		std::optional<offs_t> address = dasmView->selected_address();
		if (!address.has_value())
		{
			return;
		}
		std::string command = string_format("go 0x%X", address.value());
		m_machine.debugger().console().execute_command(command, true);
	}
}


void MainWindow::rightBarChanged(QAction *changedTo)
{
	debug_view_disasm *const dasmView = m_dasmFrame->view()->view<debug_view_disasm>();
	dasmView->set_right_column(disasm_right_column(changedTo->data().toInt()));
	m_dasmFrame->view()->viewport()->update();
}

void MainWindow::srcdbgBarChanged(QAction *changedTo)
{
	if (changedTo->data().toInt() == MENU_SHOW_SOURCE)
	{
		m_dasmFrame->setVisible(false);
		m_srcdbgFrame->setVisible(true);
		m_dasmDock->setWidget(m_srcdbgFrame);
		m_machine.debug_view().update_all(DVT_SOURCE);
	}
	else
	{
		m_dasmFrame->setVisible(true);
		m_srcdbgFrame->setVisible(false);
		m_dasmDock->setWidget(m_dasmFrame);
		m_machine.debug_view().update_all(DVT_DISASSEMBLY);
	}
}

void MainWindow::executeCommandSlot()
{
	executeCommand(true);
}

void MainWindow::commandEditedSlot(QString const &text)
{
	m_inputHistory.edit();
}

void MainWindow::executeCommand(bool withClear)
{
	QString const command = m_inputEdit->text();
	if (command == "")
	{
		// A blank command is a "silent step"
		m_machine.debugger().console().get_visible_cpu()->debug()->single_step();
		m_inputHistory.reset();
	}
	else
	{
		// Send along the command
		m_machine.debugger().console().execute_command(command.toUtf8().data(), true);

		// Add history
		m_inputHistory.add(command);

		// Clear out the text and reset the history pointer only if asked
		if (withClear)
		{
			m_inputEdit->clear();
			m_inputHistory.edit();
		}
	}
}


void MainWindow::mountImage(bool changedTo)
{
	// The image interface index was assigned to the QAction's data memeber
	const int imageIndex = dynamic_cast<QAction*>(sender())->data().toInt();
	image_interface_enumerator iter(m_machine.root_device());
	device_image_interface *img = iter.byindex(imageIndex);
	if (!img)
	{
		m_machine.debugger().console().printf("Something is wrong with the mount menu.\n");
		return;
	}

	// File dialog
	QString filename = QFileDialog::getOpenFileName(
			this,
			"Select an image file",
			QDir::currentPath(),
			tr("All files (*.*)"));

	auto [err, message] = img->load(filename.toUtf8().data());
	if (err)
	{
		m_machine.debugger().console().printf("Image could not be mounted: %s\n", !message.empty() ? message : err.message());
		return;
	}

	// Activate the unmount menu option
	QAction* unmountAct = sender()->parent()->findChild<QAction*>("unmount");
	unmountAct->setEnabled(true);

	// Set the mount name
	QMenu *parentMenuItem = dynamic_cast<QMenu *>(sender()->parent());
	QString baseString = parentMenuItem->title();
	baseString.truncate(baseString.lastIndexOf(QString(" : ")));
	const QString newTitle = baseString + QString(" : ") + QString(img->filename());
	parentMenuItem->setTitle(newTitle);

	m_machine.debugger().console().printf("Image %s mounted successfully.\n", filename.toUtf8().data());
}


void MainWindow::unmountImage(bool changedTo)
{
	// The image interface index was assigned to the QAction's data memeber
	const int imageIndex = dynamic_cast<QAction *>(sender())->data().toInt();
	image_interface_enumerator iter(m_machine.root_device());
	device_image_interface *img = iter.byindex(imageIndex);

	img->unload();

	// Deactivate the unmount menu option
	dynamic_cast<QAction *>(sender())->setEnabled(false);

	// Set the mount name
	QMenu *parentMenuItem = dynamic_cast<QMenu *>(sender()->parent());
	QString baseString = parentMenuItem->title();
	baseString.truncate(baseString.lastIndexOf(QString(" : ")));
	const QString newTitle = baseString + QString(" : ") + QString("[empty slot]");
	parentMenuItem->setTitle(newTitle);

	m_machine.debugger().console().printf("Image successfully unmounted.\n");
}


void MainWindow::dasmViewUpdated()
{
	debug_view_disasm *const dasmView = m_dasmFrame->view()->view<debug_view_disasm>();
	bool const haveCursor = dasmView->cursor_visible() && (m_machine.debugger().console().get_visible_cpu() == dasmView->source()->device());
	bool haveBreakpoint = false;
	bool breakpointEnabled = false;
	if (haveCursor)
	{
		std::optional<offs_t> const address = dasmView->selected_address();
		if (address.has_value())
		{
			device_t *const device = dasmView->source()->device();
			device_debug *const cpuinfo = device->debug();

			// Find an existing breakpoint at this address
			const debug_breakpoint *bp = cpuinfo->breakpoint_find(address.value());

			if (bp)
			{
				haveBreakpoint = true;
				breakpointEnabled = bp->enabled();
			}
		}
	}

	m_breakpointToggleAct->setText(haveBreakpoint ? "Clear Breakpoint at Cursor" : haveCursor ? "Set Breakpoint at Cursor" : "Toggle Breakpoint at Cursor");
	m_breakpointEnableAct->setText((!haveBreakpoint || breakpointEnabled) ? "Disable Breakpoint at Cursor" : "Enable Breakpoint at Cursor");
	m_breakpointToggleAct->setEnabled(haveCursor);
	m_breakpointEnableAct->setEnabled(haveBreakpoint);
	m_runToCursorAct->setEnabled(haveCursor);
}


void MainWindow::debugActClose()
{
	m_machine.schedule_exit();
}


void MainWindow::debuggerExit()
{
	// this isn't called from a Qt event loop, so close() will leak the window object
	m_exiting = true;
	delete this;
}


void MainWindow::createImagesMenu()
{
	QMenu *imagesMenu = menuBar()->addMenu("&Images");

	int interfaceIndex = 0;
	for (device_image_interface &img : image_interface_enumerator(m_machine.root_device()))
	{
		std::string menuName = string_format("%s : %s", img.device().name(), img.exists() ? img.filename() : "[empty slot]");

		QMenu *interfaceMenu = imagesMenu->addMenu(menuName.c_str());
		interfaceMenu->setObjectName(img.device().name());

		QAction *mountAct = new QAction("Mount...", interfaceMenu);
		QAction *unmountAct = new QAction("Unmount", interfaceMenu);
		mountAct->setObjectName("mount");
		mountAct->setData(QVariant(interfaceIndex));
		unmountAct->setObjectName("unmount");
		unmountAct->setData(QVariant(interfaceIndex));
		connect(mountAct, &QAction::triggered, this, &MainWindow::mountImage);
		connect(unmountAct, &QAction::triggered, this, &MainWindow::unmountImage);

		if (!img.exists())
			unmountAct->setEnabled(false);

		interfaceMenu->addAction(mountAct);
		interfaceMenu->addAction(unmountAct);

		// TODO: Cassette operations

		interfaceIndex++;
	}
}


DasmDockWidget::~DasmDockWidget()
{
}

SrcdbgDockWidget::SrcdbgDockWidget(running_machine &machine, QWidget *parent /* = nullptr */) :
	QWidget(parent),
	m_machine(machine)
{
	m_srcdbgCombo = new QComboBox(this);
	m_srcdbgView = new DebuggerView(DVT_SOURCE, m_machine, this);

	// Force a recompute of the disassembly region
	// downcast<debug_view_disasm*>(m_dasmView->view())->set_expression("curpc");

	QVBoxLayout *dvLayout = new QVBoxLayout(this);
	dvLayout->addWidget(m_srcdbgCombo);
	dvLayout->addWidget(m_srcdbgView);
	dvLayout->setContentsMargins(4,0,4,0);
	dvLayout->setSpacing(3);
// srcdbgLayout->setContentsMargins(4,0,4,2);


	// // create a combo box
	// HWND const result = CreateWindowEx(COMBO_BOX_STYLE_EX, TEXT("COMBOBOX"), nullptr, COMBO_BOX_STYLE,
	// 		0, 0, 100, 1000, parent, nullptr, GetModuleHandleUni(), nullptr);
	// SetWindowLongPtr(result, GWLP_USERDATA, userdata);
	// SendMessage(result, WM_SETFONT, (WPARAM)metrics().debug_font(), (LPARAM)FALSE);

	const debug_view_sourcecode *dvSource = downcast<debug_view_sourcecode*>(m_srcdbgView->view());
	const srcdbg_provider_base * debugInfo = dvSource->srcdbg_provider();

	if (debugInfo == nullptr)
	{
		// hide combobox when source-level debugging is off
		m_srcdbgCombo->setVisible(false);
		return;
	}

	// populate the combobox with source file paths when present
	// size_t maxlength = 0;
	std::size_t numFiles = debugInfo->num_files();
	for (std::size_t i = 0; i < numFiles; i++)
	{
		const char * entryText = debugInfo->file_index_to_path(i).built();
		// size_t const length = strlen(entry_text);
		// if (length > maxlength)
		// {
		// 	maxlength = length;
		// }
		// auto t_name = osd::text::to_tstring(entry_text);
		// SendMessage(result, CB_ADDSTRING, 0, (LPARAM) t_name.c_str());
		m_srcdbgCombo->addItem(entryText);
	}

	// SendMessage(result, CB_SETCURSEL, dv_source->cur_src_index(), 0);
	// SendMessage(result, CB_SETDROPPEDWIDTH, ((maxlength + 2) * metrics().debug_font_width()) + metrics().vscroll_width(), 0);
}

SrcdbgDockWidget::~SrcdbgDockWidget()
{
}

ProcessorDockWidget::~ProcessorDockWidget()
{
}

} // namespace osd::debugger::qt
