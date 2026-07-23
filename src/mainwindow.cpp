#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "plutointerface.h"
#include <QFile>
#include <QMessageBox>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_pluto(new PlutoInterface(this))
    , m_processor(new SignalProcessor(this))
    , m_currentFreq(5800e6)
    , m_isScanning(false)
    , m_isViewing(false)
{
    ui->setupUi(this);
    this->setWindowTitle("FPV HUNTER PRO v5.0");
    this->setMinimumSize(1200, 800);

    // Подключаем сигналы
    connect(ui->btnScan, &QPushButton::clicked, this, &MainWindow::onScanClicked);
    connect(ui->btnView, &QPushButton::clicked, this, &MainWindow::onViewClicked);
    connect(ui->btnRecord, &QPushButton::clicked, this, &MainWindow::onRecordClicked);
    connect(ui->btnSave, &QPushButton::clicked, this, &MainWindow::onSaveClicked);
    connect(ui->btnClear, &QPushButton::clicked, this, &MainWindow::onClearClicked);

    connect(ui->spinGain, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::updateGain);

    connect(m_processor, &SignalProcessor::signalDetected, this, &MainWindow::onSignalDetected);
    connect(m_processor, &SignalProcessor::spectrumUpdated, this, &MainWindow::updateSpectrum);
    connect(m_processor, &SignalProcessor::statusUpdated, this, &MainWindow::updateStatus);
    connect(m_processor, &SignalProcessor::progressUpdated, this, &MainWindow::updateProgress);

    // Подключаем Pluto
    m_pluto->connectPluto("ip:192.168.2.1");
    ui->statusBar->showMessage("✅ Готов к работе");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onScanClicked()
{
    if (m_isScanning) {
        m_processor->stopScan();
        m_isScanning = false;
        ui->btnScan->setText("▶ СКАНИРОВАТЬ");
        ui->btnScan->setStyleSheet("");
        ui->progressBar->setVisible(false);
        return;
    }

    m_isScanning = true;
    ui->btnScan->setText("⏹ СТОП");
    ui->btnScan->setStyleSheet("background-color: #e17055; color: white;");
    ui->progressBar->setVisible(true);
    ui->progressBar->setValue(0);

    SignalProcessor::ScanSettings settings;
    settings.startFreq = ui->spinStartFreq->value() * 1e6;
    settings.stopFreq = ui->spinStopFreq->value() * 1e6;
    settings.step = ui->spinStep->value() * 1e6;
    settings.gain = ui->spinGain->value();
    settings.sampleRate = ui->spinSR->value() * 1e6;

    m_processor->startScan(settings, m_pluto);
}

void MainWindow::onViewClicked()
{
    if (m_isViewing) {
        m_processor->stopView();
        m_isViewing = false;
        ui->btnView->setText("📺 ПРОСМОТР");
        ui->btnView->setStyleSheet("");
        return;
    }

    m_isViewing = true;
    ui->btnView->setText("⏹ СТОП");
    ui->btnView->setStyleSheet("background-color: #e17055; color: white;");
    m_processor->startView(m_currentFreq, m_pluto);
}

void MainWindow::onRecordClicked()
{
    // Запись видео
}

void MainWindow::onSaveClicked()
{
    // Сохранение кадра
}

void MainWindow::onClearClicked()
{
    ui->listSignals->clear();
}

void MainWindow::updateGain()
{
    if (m_pluto->isConnected()) {
        m_pluto->setGain(ui->spinGain->value());
    }
}

void MainWindow::onSignalDetected(const SignalInfo &signal)
{
    QString text = QString("%1 %2 МГц | %3 | %4 dBFS")
        .arg(signal.type.contains("видео") ? "📡" : "🎮")
        .arg(signal.freq / 1e6, 0, 'f', 2)
        .arg(signal.type)
        .arg(signal.power, 0, 'f', 1);

    QListWidgetItem *item = new QListWidgetItem(text);
    if (signal.type.contains("видео")) {
        item->setForeground(QColor(0, 200, 100));
    } else {
        item->setForeground(QColor(255, 200, 50));
    }

    ui->listSignals->insertItem(0, item);
    ui->listSignals->setCurrentRow(0);

    m_currentFreq = signal.freq;
    ui->btnView->setEnabled(true);
    ui->btnRecord->setEnabled(true);
    ui->btnSave->setEnabled(true);

    ui->labelFreq->setText(QString("Частота: %1 МГц").arg(signal.freq / 1e6, 0, 'f', 2));
    ui->labelInfo->setText(QString("📡 %1 | %2 dBFS").arg(signal.type).arg(signal.power, 0, 'f', 1));
}

void MainWindow::updateSpectrum(const QVector<double> &freq, const QVector<double> &power)
{
    // Обновление графика
    ui->widgetSpectrum->setData(freq, power);
}

void MainWindow::updateStatus(const QString &status)
{
    ui->statusBar->showMessage("🔹 " + status);
}

void MainWindow::updateProgress(int value)
{
    ui->progressBar->setValue(value);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_processor->stop();
    event->accept();
}
