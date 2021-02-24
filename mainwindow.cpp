#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QFileDialog>
#include <QInputDialog>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QVector>

#include <QFuture>
#include <QtConcurrent>
#include <QtWidgets>

#include "appversion.h"

static const QString defaultStatusbarMsg("Author: Rainer Semma");

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, mFilename()
{
	ui->setupUi(this);
	QHeaderView *header = ui->tableWidget->horizontalHeader();
	header->setSectionResizeMode(0, QHeaderView::Stretch);
	ui->tableWidget->setColumnWidth(1, 100);
//	header->setSectionResizeMode(1, QHeaderView::Interactive);
	ui->statusbar->showMessage(tr(defaultStatusbarMsg.toStdString().c_str()));
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::on_pushButton_released()
{
	mFilename = QFileDialog::getOpenFileName(this, "Open file", QDir::homePath(), tr("Text files (*.txt *.log)"));
	if (mFilename.isEmpty())
		return;

	qDebug() << "selected file:" << mFilename;
	ui->statusbar->showMessage(mFilename);

	/* reset counter */
	resetCounter();
}

void MainWindow::on_pushButton_2_released()
{
	bool ok(false);
	const QString text = QInputDialog::getText(this, tr("Add new filter"), tr("enter new filter"), QLineEdit::Normal, QString(), &ok);
	if (!ok) {
		return;
	}
	const int row(this->ui->tableWidget->rowCount());
	this->ui->tableWidget->insertRow(row);
	/* add filter */
	this->ui->tableWidget->setItem(row, 0, new QTableWidgetItem(text));
	/* add count */
	this->ui->tableWidget->setItem(row, 1, new QTableWidgetItem("0"));

	ui->statusbar->showMessage("added filter", 3000);
}

void MainWindow::resetCounter()
{
	for (int i=0; i<this->ui->tableWidget->rowCount(); ++i) {
		QTableWidgetItem *item = this->ui->tableWidget->item(i, 1);
		item->setText("0");
	}
}

void MainWindow::on_pushButton_analyse_released()
{
	/* sanity checks */
	if (this->ui->tableWidget->rowCount() <= 0) {
		qDebug() << "Nothing to do";
		ui->statusbar->showMessage(tr("Nothing to do"), 3000);
		return;
	}
	if (mFilename.isEmpty()) {
		qDebug() << "No file to analyse";
		ui->statusbar->showMessage(tr("No file to analyse"), 3000);
		return;
	}
	if (!QFile::exists(mFilename)) {
		qDebug() << "Selected file not found" << mFilename;
		ui->statusbar->showMessage(tr("Selected file not found"), 3000);
		return;
	}

	/* disable user edit */

	/* reset counter */
	resetCounter();

	/* start analysis */
	ui->statusbar->showMessage(tr("Running analysis"));
	QFile inputFile(mFilename);
	if (inputFile.open(QIODevice::ReadOnly))
	{
		QTextStream in(&inputFile);
		QStringList buffer;
		const int buffersize(100);
		while (!in.atEnd())
		{
			const QString line = in.readLine();
			buffer.append(line);
			if (in.atEnd() || buffer.size() >= buffersize) {
				QVector<QFuture<int> > futureList;
				for (int i=0; i<this->ui->tableWidget->rowCount(); ++i) {
					futureList.append(QtConcurrent::run([&](const QString &substr, const QStringList &list) {
						int cnt(0);
						foreach (const QString &line, list) {
							cnt += line.count(substr);
						}
						return cnt;
					},
												this->ui->tableWidget->item(i, 0)->text(),
												buffer));
				}
				for (int i=0; i<futureList.size(); ++i) {
					const int found = futureList.at(i).result();
					QTableWidgetItem *item = this->ui->tableWidget->item(i, 1);
					bool ok(false);
					int cnt = item->text().toInt(&ok);
					if (ok) {
						item->setText(QString::number(cnt + found));
					} else {
						qDebug() << "Error: failed to parse" << item->text();
					}
				}
				buffer.clear();
			}
		}
		inputFile.close();
	}
	ui->statusbar->showMessage(tr(defaultStatusbarMsg.toStdString().c_str()));

	/* enable user edit */
}

void MainWindow::on_pushButton_3_released()
{
	QSet<int> rows;
	QList<QTableWidgetItem *> list = this->ui->tableWidget->selectedItems();
	for (QList<QTableWidgetItem*>::const_iterator iter = list.constBegin(); iter != list.constEnd(); ++iter) {
		rows.insert((*iter)->row());
	}
	auto r = rows.toList();
	for (int i = r.size() - 1; i >= 0; --i) {
		ui->tableWidget->removeRow(r[i]);
	}
}

void MainWindow::on_actionQuit_triggered()
{
	QApplication::quit();
}

void MainWindow::on_action_about_LogItemCounter_triggered()
{
	QMessageBox mb;
	mb.setText("Version: " APPVERSION);
	mb.exec();
}
