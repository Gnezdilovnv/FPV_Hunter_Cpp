#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "signalprocessor.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class PlutoInterface;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onScanClicked();
    void onViewClicked();
    void onRecordClicked();
    void onSaveClicked();
    void onClearClicked();
    void onSignalDetected(const SignalInfo &signal);
    void updateSpectrum(const QVector<double> &freq, const QVector<double> &power);
    void updateStatus(const QString &status);
    void updateProgress(int value);
    void updateGain();

private:
    Ui::MainWindow *ui;
    PlutoInterface *m_pluto;
    SignalProcessor *m_processor;
    double m_currentFreq;
    bool m_isScanning;
    bool m_isViewing;
};

#endif // MAINWINDOW_H
