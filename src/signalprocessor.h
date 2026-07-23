#ifndef SIGNALPROCESSOR_H
#define SIGNALPROCESSOR_H

#include <QObject>
#include <QVector>
#include <complex>
#include <QMutex>

class PlutoInterface;

struct SignalInfo {
    double freq;
    double power;
    double bandwidth;
    QString type;
    QString timestamp;
};

class SignalProcessor : public QObject
{
    Q_OBJECT

public:
    struct ScanSettings {
        double startFreq = 2400e6;
        double stopFreq = 6000e6;
        double step = 5e6;
        double gain = 40;
        double sampleRate = 2e6;
    };

    explicit SignalProcessor(QObject *parent = nullptr);
    ~SignalProcessor();

    void startScan(const ScanSettings &settings, PlutoInterface *pluto);
    void stopScan();
    void startView(double freq, PlutoInterface *pluto);
    void stopView();
    void stop();
    bool isRunning() const { return m_running; }

signals:
    void signalDetected(const SignalInfo &signal);
    void spectrumUpdated(const QVector<double> &freq, const QVector<double> &power);
    void statusUpdated(const QString &status);
    void progressUpdated(int value);

private:
    void processScan();
    void processView();
    double estimateBandwidth(const QVector<std::complex<float>> &samples);
    QString detectType(double freq, double bw);

    volatile bool m_running;
    volatile bool m_scanning;
    volatile bool m_viewing;
    ScanSettings m_settings;
    double m_viewFreq;
    PlutoInterface *m_pluto;
    QMutex m_mutex;
};

#endif // SIGNALPROCESSOR_H
