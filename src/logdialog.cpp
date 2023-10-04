#include "logdialog.h"
#include "ui_logdialog.h"
#include "loghandler.h"
#include "utils.h"
#include <QMenu>
#include <QClipboard>

LogDialog::LogDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LogDialog)
{
    ui->setupUi(this);

#ifdef Q_OS_WINDOWS
    AxelChat::setDarkWindowFrame(winId());
#endif

    setWindowFlags(Qt::Window);

    model = LogHandler::getModel();
    ui->messages->setModel(model);

    ui->messages->setColumnWidth(0, 1200);
    ui->messages->setColumnWidth(1, 300);
    ui->messages->setColumnWidth(2, 400);

    connect(ui->messages, QOverload<const QPoint &>::of(&QWidget::customContextMenuRequested), this, [this](const QPoint & pos)
    {
        QMenu *menu = new QMenu(this);
        QAction* action;

        {
            action = new QAction(tr("Copy"), menu);
            action->setShortcuts(QKeySequence::StandardKey::Copy);
            action->setEnabled(ui->messages->currentIndex().isValid());
            connect(action, &QAction::triggered, this, [this]()
            {
                if (const QModelIndex index = ui->messages->currentIndex(); index.isValid())
                {
                    const QString text = model->data(index, Qt::DisplayRole).toString();
                    QGuiApplication::clipboard()->setText(text);
                }
            });
            menu->addAction(action);
        }

        {
            action = new QAction(tr("Remove selected"), menu);
            //action->setShortcuts(QKeySequence::StandardKey::Delete);
            action->setEnabled(!ui->messages->selectionModel()->selectedRows().isEmpty());
            connect(action, &QAction::triggered, this, [this]()
            {
                QItemSelectionModel *selection = ui->messages->selectionModel();
                if (!selection)
                {
                    qWarning() << "selection model is null";
                    return;
                }

                const QModelIndexList rows = selection->selectedRows();
                for (int i = rows.count() - 1; i >= 0; --i)
                {
                    model->removeRow(rows.at(i).row());
                }
            });
            menu->addAction(action);
        }

        menu->addSeparator();

        {
            action = new QAction(tr("Clear all"), menu);
            connect(action, &QAction::triggered, this, [this]()
            {
                model->clear();
            });
            menu->addAction(action);
        }

        menu->popup(ui->messages->viewport()->mapToGlobal(pos));
    });
}

LogDialog::~LogDialog()
{
    delete ui;
}
