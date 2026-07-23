#include "signalprocessor.h"
#include "plutointerface.h"
#include <QThread>
#include <QDateTime>
#include <cmath>
#include <QDebug>

SignalProcessor::SignalProcessor(QObject *parent)
    : QObject(parent)
    , m_running(false)
    , m_scanning(false)
    , m_viewing(false)
    , m_viewFreq(5800e6)
    , m_pluto(nullptr)
{
}

SignalProcessor::~SignalProcessor()
{
    stop();
}

void SignalProcessor::startScan(const ScanSettings &settings, PlutoInterface *pluto)
{
    if (m_running) return;

    m_settings = settings;
    m_pluto = pluto;
    m_scanning = true;
    m_running = true;

    emit statusUpdated("🔍 Сканирование запущено...");
    QThread::create([this]() { processScan(); })->start();
}

void SignalProcessor::stopScan()
{
    m_scanning = false;
    m_running = false;
    emit statusUpdated("⏹ Сканирование остановлено");
}

void SignalProcessor::startView(double freq, PlutoInterface *pluto)
{
    if (m_viewing) return;

    m_viewFreq = freq;
    m_pluto = pluto;
    m_viewing = true;
    m_running = true;

    emit statusUpdated(QString("📺 Просмотр %1 МГц").arg(freq / 1e6, 0, 'f', 2));
    QThread::create([this]() { processView(); })->start();
}

void SignalProcessor::stopView()
{
    m_viewing = false;
    m_running = false;
    emit statusUpdated("⏹ Просмотр остановлен");
}

void SignalProcessor::stop()
{
    m_running = false;
    m_scanning = false;
    m_viewing = false;
}

void SignalProcessor::processScan()
{
    double start = m_settings.startFreq;
    double stop = m_settings.stopFreq;
    double step = m_settings.step;
    int total = (int)((stop - start) / step);

    for (int i = 0; i < total && m_scanning && m_running; ++i) {
        double freq = start + i * step;

        if (!m_pluto || !m_pluto->isConnected()) {
            emit statusUpdated("⚠️ Pluto не подключен");
            break;
        }

        m_pluto->setFrequency(freq);
        QThread::msleep(20);

        auto samples = m_pluto->receiveSamples(1024);
        if (samples.isEmpty()) continue;

        double power = 0;
        for (const auto &s : samples) {
            power += std::norm(s);
        }
        power = 10 * log10(power / samples.size() + 1e-12);

        emit progressUpdated((int)((i + 1) * 100.0 / total));

        if (power > -50) {
            double bw = estimateBandwidth(samples);
            QString type = detectType(freq, bw);

            SignalInfo info;
            info.freq = freq;
            info.power = power;
            info.bandwidth = bw;
            info.type = type;
            info.timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");

            emit signalDetected(info);

            if (type.contains("видео")) {
                m_scanning = false;
                emit statusUpdated(QString("📡 Видео на %1 МГц").arg(freq / 1e6, 0, 'f', 2));
                break;
            }
        }

        if (i % 10 == 0) {
            emit statusUpdated(QString("🔍 %1 МГц").arg(freq / 1e6, 0, 'f', 0));
        }
    }

    m_scanning = false;
    m_running = false;
    emit statusUpdated("✅ Сканирование завершено");
}

void SignalProcessor::processView()
{
    while (m_viewing && m_running) {
        if (!m_pluto || !m_pluto->isConnected()) {
            emit statusUpdated("⚠️ Pluto не подключен");
            QThread::msleep(100);
            continue;
        }

        m_pluto->setFrequency(m_viewFreq);
        auto samples = m_pluto->receiveSamples(512);
        if (samples.isEmpty()) continue;

        QVector<double> freq, power;
        int n = samples.size();

        for (int i = 0; i < n; ++i) {
            double f = (i - n/2) * m_settings.sampleRate / n;
            freq.append(f / 1e6);
            power.append(20 * log10(std::abs(samples[i]) + 1e-12));
        }

        emit spectrumUpdated(freq, power);
        QThread::msleep(50);
    }
}

double SignalProcessor::estimateBandwidth(const QVector<std::complex<float>> &samples)
{
    int n = samples.size();
    QVector<double> power(n);
    double max_power = 0;

    for (int i = 0; i < n; ++i) {
        power[i] = std::norm(samples[i]);
        if (power[i] > max_power) max_power = power[i];
    }

    double threshold = max_power * 0.3;
    int start = 0, end = n - 1;

    for (int i = 0; i < n; ++i) {
        if (power[i] > threshold) { start = i; break; }
    }
    for (int i = n - 1; i >= 0; --i) {
        if (power[i] > threshold) { end = i; break; }
    }

    if (end > start) {
        return (end - start) * m_settings.sampleRate / n;
    }
    return 0;
}

QString SignalProcessor::detectType(double freq, double bw)
{
    if (freq >= 5700e6 && freq <= 5900e6) {
        return bw < 10e6 ? "видео (аналоговый)" : "видео (цифровой)";
    } else if (freq >= 2400e6 && freq <= 2483e6) {
        return "пульт управления";
    } else if (freq >= 900e6 && freq <= 930e6) {
        return "пульт (900 МГц)";
    }
    return "неизвестный";
}
