#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();

private slots:
	void on_pushButton_released();

	void on_pushButton_2_released();

	void on_pushButton_analyse_released();

	void on_pushButton_3_released();

private:
	Ui::MainWindow *ui;
	QString mFilename;

	void resetCounter();
};
#endif // MAINWINDOW_H
