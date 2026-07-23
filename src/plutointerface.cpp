#include "plutointerface.h"
#include <QDebug>
#include <QThread>

PlutoInterface::PlutoInterface(QObject *parent)
    : QObject(parent)
    , m_ctx(nullptr)
    , m_phy(nullptr)
    , m_rx(nullptr)
    , m_rx_channel(nullptr)
    , m_rx_buffer(nullptr)
    , m_connected(false)
    , m_sampleRate(2.0e6)
    , m_gain(40.0)
{
}

PlutoInterface::~PlutoInterface()
{
    disconnectPluto();
}

bool PlutoInterface::connectPluto(const QString &uri)
{
    if (m_connected) {
        disconnectPluto();
    }

    m_ctx = iio_create_context_from_uri(uri.toUtf8().constData());
    if (!m_ctx) {
        emit errorOccurred("Не удалось подключиться к Pluto");
        return false;
    }

    m_phy = iio_context_find_device(m_ctx, "ad9361-phy");
    m_rx = iio_context_find_device(m_ctx, "cf-ad9361-lpc");

    if (!m_phy || !m_rx) {
        iio_context_destroy(m_ctx);
        m_ctx = nullptr;
        emit errorOccurred("Устройства AD9361 не найдены");
        return false;
    }

    m_rx_channel = iio_device_find_channel(m_rx, "voltage0", false);
    if (!m_rx_channel) {
        emit errorOccurred("Канал приёма не найден");
        return false;
    }

    iio_channel_enable(m_rx_channel);

    setSampleRate(m_sampleRate);
    setGain(m_gain);

    m_connected = true;
    emit connected();

    qDebug() << "✅ Pluto+ подключен";
    return true;
}

void PlutoInterface::disconnectPluto()
{
    if (m_rx_buffer) {
        iio_buffer_destroy(m_rx_buffer);
        m_rx_buffer = nullptr;
    }
    if (m_ctx) {
        iio_context_destroy(m_ctx);
        m_ctx = nullptr;
    }
    m_connected = false;
    emit disconnected();
    qDebug() << "Pluto отключен";
}

bool PlutoInterface::setFrequency(double freqHz)
{
    if (!m_connected || !m_phy) return false;
    int ret = iio_device_attr_write_double(m_phy, "RX_LO_FREQ", freqHz);
    if (ret < 0) {
        emit errorOccurred(QString("Ошибка настройки частоты: %1").arg(ret));
        return false;
    }
    return true;
}

bool PlutoInterface::setSampleRate(double rateHz)
{
    if (!m_connected || !m_phy) return false;
    m_sampleRate = rateHz;
    int ret = iio_device_attr_write_double(m_phy, "RX_SAMPLING_FREQ", rateHz);
    if (ret < 0) {
        emit errorOccurred(QString("Ошибка настройки частоты дискретизации: %1").arg(ret));
        return false;
    }
    iio_device_attr_write_double(m_rx, "RX_RF_BANDWIDTH", rateHz);
    return true;
}

bool PlutoInterface::setGain(double gainDb)
{
    if (!m_connected || !m_phy) return false;
    m_gain = gainDb;
    iio_device_attr_write(m_phy, "RX_GAIN_MODE", "manual");
    int ret = iio_device_attr_write_double(m_phy, "RX_GAIN", gainDb);
    if (ret < 0) {
        emit errorOccurred(QString("Ошибка настройки усиления: %1").arg(ret));
        return false;
    }
    return true;
}

bool PlutoInterface::setBandwidth(double bwHz)
{
    if (!m_connected || !m_rx) return false;
    return iio_device_attr_write_double(m_rx, "RX_RF_BANDWIDTH", bwHz) >= 0;
}

QVector<std::complex<float>> PlutoInterface::receiveSamples(size_t count)
{
    QVector<std::complex<float>> result;
    if (!m_connected || !m_rx_channel) return result;

    try {
        if (m_rx_buffer) {
            iio_buffer_destroy(m_rx_buffer);
        }
        m_rx_buffer = iio_device_create_buffer(m_rx, count, false);
        if (!m_rx_buffer) {
            emit errorOccurred("Не удалось создать буфер");
            return result;
        }

        ssize_t bytes = iio_buffer_refill(m_rx_buffer);
        if (bytes < 0) {
            emit errorOccurred("Ошибка заполнения буфера");
            return result;
        }

        size_t sample_count = bytes / sizeof(int16_t) / 2;
        result.reserve(sample_count);
        int16_t *data = (int16_t *)iio_buffer_first(m_rx_buffer, m_rx_channel);

        for (size_t i = 0; i < sample_count; ++i) {
            float i_val = data[i * 2] / 2048.0f;
            float q_val = data[i * 2 + 1] / 2048.0f;
            result.push_back(std::complex<float>(i_val, q_val));
        }
    } catch (...) {
        emit errorOccurred("Исключение при приёме данных");
    }
    return result;
}
