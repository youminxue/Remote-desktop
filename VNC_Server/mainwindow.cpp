#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtGui>
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->keyLineEdit->setReadOnly(true);
    server = new QVncServer(QGuiApplication::primaryScreen());
}

MainWindow::~MainWindow()
{
    delete ui;
    delete server;
}
