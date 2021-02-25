#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QFileDialog>
#include <QInputDialog>
#include <QProgressDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QVector>
#include <QByteArray>

#include <QFuture>
#include <QtConcurrent>
#include <QtWidgets>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "appversion.h"

static const QString defaultStatusbarMsg("Author: Rainer Semma");

static bool saveFiltersToFile(const QString &filename, const QStringList &filterList)
{
	QFile saveFile(filename);
	if (!saveFile.open(QIODevice::WriteOnly)) {
		qDebug() << "failed to save filters to" << saveFile.fileName();
		return false;
	}

	QJsonObject settings;
	settings["version"] = APPVERSION;
	QJsonArray filters;
	foreach (const QString &filter, filterList) {
		filters.append(filter);
	}
	settings["filters"] = filters;
	QJsonDocument saveDoc(settings);
	saveFile.write(saveDoc.toJson());

	saveFile.close();

	return true;
}

static bool readFiltersFromFile(const QString &filename, QStringList &filterList)
{
	filterList.clear();

	QFile loadFile(filename);
	if (!loadFile.open(QIODevice::ReadOnly)) {
		qDebug() << "failed to load filters from" << loadFile.fileName();
		return false;
	}

	QByteArray ba = loadFile.readAll();
	QJsonDocument loadDoc(QJsonDocument::fromJson(ba));
	QString version = loadDoc.object()["version"].toString();
	if (version != APPVERSION) {
		qDebug() << "mismatch in settings version";
	} else {
		qDebug() << "match in settings version";
	}
	QJsonArray filters = loadDoc.object()["filters"].toArray();
	while (!filters.isEmpty()) {
		filterList.append(filters.takeAt(0).toString());
	}

	loadFile.close();

	return true;
}

template<class T>
class PropertyLoadAndSaveHelper {
	QString mKey;
	T mValue;
public:
	PropertyLoadAndSaveHelper(const QString &key, const T &defaultValue)
		: mKey(key)
		, mValue(defaultValue)
	{
		const QVariant var = QApplication::instance()->property(key.toStdString().c_str());
		if (var.isValid()) {
			if (var.canConvert<T>()) {
				mValue = var.value<T>();
			}
		}
	}

	T getValue() const
	{
		return mValue;
	}

	void update(const T &value)
	{
		mValue = value;
	}

	~PropertyLoadAndSaveHelper()
	{
		(void)QApplication::instance()->setProperty(mKey.toStdString().c_str(), QVariant::fromValue(mValue));
	}
};

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, mFilename(QDir::homePath())
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

/*!
 * \brief set selected (workon) file
 */
void MainWindow::on_pushButton_released()
{
	PropertyLoadAndSaveHelper<QString> plash("selected_file", mFilename);

	mFilename = QFileDialog::getOpenFileName(this, "Open file", plash.getValue(), tr("Text files (*.txt *.log)"));
	if (mFilename.isEmpty())
		return;
	plash.update(QFileInfo(mFilename).absoluteDir().absolutePath());

	qDebug() << "selected file:" << mFilename;
	ui->statusbar->showMessage(mFilename);

	/* reset counter */
	resetCounter();
}

/*!
 * \brief	add filter to list
 */
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

/*!
 * \brief	analyse selected file with given filters
 */
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
	if (!QFileInfo(mFilename).isFile()) {
		qDebug() << "Selected type is not a file" << mFilename;
		ui->statusbar->showMessage(tr("No file selected"), 3000);
		return;
	}
	if (!QFileInfo(mFilename).isReadable()) {
		qDebug() << "Selected file is not readable" << mFilename;
		ui->statusbar->showMessage(tr("Selected file is not readable"), 3000);
		return;
	}

	/* disable user edit */
	QProgressDialog progress("Analysing file...", "Abort", 0, 100, this);
	progress.setWindowModality(Qt::WindowModal);

	/* reset counter */
	resetCounter();

	/* start analysis */
	ui->statusbar->showMessage(tr("Running analysis"));
	QFile inputFile(mFilename);
	qint64 size(QFile(mFilename).size());
	if (inputFile.open(QIODevice::ReadOnly))
	{
		QTextStream in(&inputFile);
		QStringList buffer;
		const int buffersize(100);
		int lineCnt(0);
		while (!in.atEnd() && !progress.wasCanceled())
		{
			const QString line = in.readLine();
			buffer.append(line);
			++lineCnt;
			progress.setValue(static_cast<int>((lineCnt * 200. * 100.) / static_cast<double>(size)));
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
	if (progress.wasCanceled()) {
		qDebug() << "analyse was canceled";
		ui->statusbar->showMessage(tr("analyse was canceled"));
		resetCounter();
	} else {
		progress.setValue(100);
		ui->statusbar->showMessage(tr(defaultStatusbarMsg.toStdString().c_str()));
	}

	/* enable user edit */
}

/*!
 * \brief	remove selected rows
 */
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

/*!
 * \brief	quit application
 */
void MainWindow::on_actionQuit_triggered()
{
	QApplication::quit();
}

/*!
 * \brief	show about infos
 */
void MainWindow::on_action_about_LogItemCounter_triggered()
{
	QMessageBox mb;
	mb.setText("Version: " APPVERSION);
	mb.exec();
}

/*!
 * \brief	save filters
 */
void MainWindow::on_actionSave_Filter_triggered()
{
	PropertyLoadAndSaveHelper<QString> plash("filter_file", QDir::homePath());
	QString filename = QFileDialog::getSaveFileName(this, tr("Save Filter File"), plash.getValue(), tr("Filter Files (*.licf)"));
	if (filename.isEmpty())
		return;
	plash.update(filename);

	QStringList filters;
	for (int i=0; i<this->ui->tableWidget->rowCount(); ++i) {
		QTableWidgetItem *item = this->ui->tableWidget->item(i, 0);
		filters.append(item->text());
	}
	if (!saveFiltersToFile(filename, filters)) {
		qDebug() << "save failed";
	}
}

void MainWindow::on_actionLoad_Filter_triggered()
{
	PropertyLoadAndSaveHelper<QString> plash("filter_file", QDir::homePath());
	QString filename = QFileDialog::getOpenFileName(this, tr("Load Filter File"), plash.getValue(), tr("Filter Files (*.licf)"));
	if (filename.isEmpty())
		return;
	plash.update(filename);

	QStringList filters;
	if (!readFiltersFromFile(filename, filters)) {
		qDebug() << "load failed";
		return;
	}
	/* clean table */
	while (ui->tableWidget->rowCount()) {
		ui->tableWidget->removeRow(0);
	}
	/* fill table */
	foreach (const QString &filter, filters) {
		const int row(this->ui->tableWidget->rowCount());
		this->ui->tableWidget->insertRow(row);
		/* add filter */
		this->ui->tableWidget->setItem(row, 0, new QTableWidgetItem(filter));
		/* add count */
		this->ui->tableWidget->setItem(row, 1, new QTableWidgetItem("0"));
	}
}
