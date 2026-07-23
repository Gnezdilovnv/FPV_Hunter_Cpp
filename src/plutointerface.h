#ifndef PLUTOINTERFACE_H
#define PLUTOINTERFACE_H

#include <QObject>
#include <QVector>
#include <complex>

#ifdef WIN32
#include <iio.h>
#else
#include <iio/iio.h>
#endif

class PlutoInterface : public QObject
{
    Q_OBJECT

public:
    explicit PlutoInterface(QObject *parent = nullptr);
    ~PlutoInterface();

    bool connectPluto(const QString &uri = "ip:192.168.2.1");
    void disconnectPluto();
    bool isConnected() const { return m_connected; }

    bool setFrequency(double freqHz);
    bool setSampleRate(double rateHz);
    bool setGain(double gainDb);
    bool setBandwidth(double bwHz);

    QVector<std::complex<float>> receiveSamples(size_t count = 4096);

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);

private:
    struct iio_context *m_ctx;
    struct iio_device *m_phy;
    struct iio_device *m_rx;
    struct iio_channel *m_rx_channel;
    struct iio_buffer *m_rx_buffer;
    bool m_connected;
    double m_sampleRate;
    double m_gain;
};

#endif // PLUTOINTERFACE_H
