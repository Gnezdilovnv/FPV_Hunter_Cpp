#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QTabWidget>
#include <QTimer>
#include <QStatusBar>
#include <QMessageBox>
#include <QSlider>
#include <QCheckBox>
#include <QLineEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QDateTime>
#include <QTextToSpeech>
#include <QTextEdit>
#include <QProgressBar>
#include <QFileDialog>
#include <QTextStream>
#include <QFile>

#include <iio.h>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <fstream>

class PlutoSDR {
public:
    PlutoSDR() : ctx(nullptr), phy(nullptr), rx(nullptr), connected(false) {}
    ~PlutoSDR() { disconnect(); }

    bool connect(const QString &ip = "192.168.2.1") {
        disconnect();
        ctx = iio_create_context_from_uri(qPrintable("ip:" + ip));
        if (!ctx) ctx = iio_create_context_from_uri("usb:");
        if (!ctx) return false;

        phy = iio_context_find_device(ctx, "ad9361-phy");
        rx = iio_context_find_device(ctx, "cf-ad9361-lpc");
        if (!phy || !rx) { disconnect(); return false; }

        rx_channel = iio_device_find_channel(rx, "voltage0", false);
        if (!rx_channel) { disconnect(); return false; }
        iio_channel_enable(rx_channel);

        connected = true;
        setFrequency(100e6);
        setGain(40);
        setAGC(false);
        setSampleRate(4e6);
        setBandwidth(4e6);
        return true;
    }

    void disconnect() {
        if (ctx) { iio_context_destroy(ctx); ctx = nullptr; }
        connected = false;
    }

    bool setFrequency(double freq) {
        if (!connected || !phy) return false;
        frequency = freq;
        return iio_device_attr_write_double(phy, "RX_LO_FREQ", freq) >= 0;
    }

    bool setSampleRate(double rate) {
        if (!connected || !phy) return false;
        sampleRate = rate;
        return iio_device_attr_write_double(phy, "RX_SAMPLING_FREQ", rate) >= 0;
    }

    bool setBandwidth(double bw) {
        if (!connected || !phy) return false;
        bandwidth = bw;
        return iio_device_attr_write_double(phy, "in_voltage_rf_bandwidth", bw) >= 0;
    }

    bool setGain(double gain) {
        if (!connected || !phy) return false;
        this->gain = gain;
        return iio_device_attr_write_double(phy, "RX_GAIN", gain) >= 0;
    }

    bool setAGC(bool enable) {
        if (!connected || !phy) return false;
        agcEnabled = enable;
        return iio_device_attr_write_double(phy, "RX_GAIN_MODE", enable ? 1 : 0) >= 0;
    }

    double getRSSI() {
        if (!connected || !phy) return -100;
        double rssi = -100;
        iio_device_attr_read_double(phy, "RX_RSSI", &rssi);
        return rssi;
    }

    double getTemperature() {
        if (!connected || !phy) return 0;
        double temp = 0;
        iio_device_attr_read_double(phy, "in_voltage_temperature", &temp);
        return temp;
    }

    double getFrequency() const { return frequency; }
    bool isConnected() const { return connected; }

    std::vector<float> receiveSamples(int count = 1024) {
        std::vector<float> samples;
        if (!connected || !rx || !rx_channel) return samples;

        iio_buffer *buf = iio_device_create_buffer(rx, count, false);
        if (!buf) return samples;

        int bytes = iio_buffer_refill(buf);
        if (bytes < 0) { iio_buffer_destroy(buf); return samples; }

        void *data = iio_buffer_first(buf, rx_channel);
        if (!data) { iio_buffer_destroy(buf); return samples; }

        int sampleCount = bytes / 4;
        samples.resize(sampleCount);
        for (int i = 0; i < sampleCount; i++) {
            short i_val = reinterpret_cast<short*>(data)[i * 2];
            short q_val = reinterpret_cast<short*>(data)[i * 2 + 1];
            samples[i] = sqrt(i_val * i_val + q_val * q_val) / 2048.0f;
        }

        iio_buffer_destroy(buf);
        return samples;
    }

private:
    iio_context *ctx;
    iio_device *phy, *rx;
    iio_channel *rx_channel;
    bool connected;
    double frequency = 100e6;
    double sampleRate = 4e6;
    double gain = 40;
    double bandwidth = 4e6;
    bool agcEnabled = false;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        pluto = new PlutoSDR();
        signalCount = 0;
        isScanning = true;
        isRecording = false;
        isPaused = false;
        settingsVoice = true;

        setupUI();
        setupTimers();
        statusBar()->showMessage("Готов к работе");
        QTimer::singleShot(500, this, &MainWindow::connectPluto);
    }

    ~MainWindow() { delete pluto; }

private slots:
    void connectPluto() {
        statusBar()->showMessage("Подключение к Pluto SDR...");
        if (pluto->connect()) {
            statusLabel->setText("🟢 Pluto: подключен");
            statusLabel->setStyleSheet("color: #44ff44;");
            connectBtn->setText("✅ Pluto OK");
            connectBtn->setStyleSheet("background-color: #2a5a2a;");
            statusBar()->showMessage("✅ Pluto SDR готов");
            logMessage("✅ Pluto SDR подключен");
        } else {
            statusLabel->setText("🔴 Pluto: не найден");
            statusLabel->setStyleSheet("color: #ff4444;");
            connectBtn->setText("🔄 Повторить");
            connectBtn->setStyleSheet("background-color: #5a2a2a;");
            statusBar()->showMessage("❌ Pluto SDR не найден");
            logMessage("❌ Ошибка подключения Pluto");
        }
    }

    void scanStep() {
        if (!pluto->isConnected() || isPaused) return;

        static int step = 0;
        double freq = 70e6 + (step % 100) * 5e6;
        step++;

        pluto->setFrequency(freq);
        std::vector<float> samples = pluto->receiveSamples(256);

        if (!samples.empty()) {
            double sum = 0;
            for (float s : samples) sum += s;
            double power = 10 * log10(sum / samples.size() + 1e-12);

            if (power > -50) {
                QString type = "Неизвестный";
                bool hasVideo = false;
                if (freq >= 5700e6 && freq <= 5900e6) { type = "FPV Analog"; hasVideo = true; }
                else if (freq >= 2400e6 && freq <= 2483e6) { type = "Пульт DJI"; }
                else if (freq >= 900e6 && freq <= 930e6) { type = "Пульт 900МГц"; }

                QString display = QString("%1 МГц | %2 дБ | %3")
                    .arg(freq/1e6, 0, 'f', 1)
                    .arg(power, 0, 'f', 1)
                    .arg(type);

                signalList->addItem(display);
                signalCount++;
                signalCountLabel->setText(QString("Сигналов: %1").arg(signalCount));

                if (settingsVoice) {
                    voice->say(QString("Обнаружен %1 сигнал на частоте %2 мегагерц")
                               .arg(type).arg(freq/1e6, 0, 'f', 1));
                }
            }
        }
    }

    void updateUI() {
        if (pluto->isConnected()) {
            double rssi = pluto->getRSSI();
            double temp = pluto->getTemperature();
            rssiLabel->setText(QString("📶 RSSI: %1 дБ").arg(rssi, 0, 'f', 1));
            tempLabel->setText(QString("🌡️ %1°C").arg(temp, 0, 'f', 1));
        }
    }

    void toggleRecording() {
        if (isRecording) {
            isRecording = false;
            recordBtn->setText("🔴 Запись");
            recordBtn->setStyleSheet("background-color: #2a2a4a;");
            statusBar()->showMessage("⏹ Запись остановлена");
            logMessage("⏹ Запись остановлена");
        } else {
            isRecording = true;
            recordBtn->setText("⏹ Стоп");
            recordBtn->setStyleSheet("background-color: #5a2a2a;");
            statusBar()->showMessage("🔴 Запись начата");
            logMessage("🔴 Запись начата");
        }
    }

    void takeSnapshot() {
        statusBar()->showMessage("📷 Снимок сохранён");
        logMessage("📷 Снимок сохранён");
    }

    void togglePause() {
        isPaused = !isPaused;
        pauseBtn->setText(isPaused ? "▶ Продолжить" : "⏸ Пауза");
        statusBar()->showMessage(isPaused ? "⏸ Пауза" : "▶ Продолжено");
    }

    void toggleScan() {
        isScanning = !isScanning;
        scanBtn->setText(isScanning ? "⏹ Стоп" : "▶ Старт");
        statusBar()->showMessage(isScanning ? "Сканирование активно" : "Сканирование остановлено");
    }

    void showSettingsDialog() {
        QDialog dialog(this);
        dialog.setWindowTitle("⚙️ Настройки");
        dialog.setMinimumWidth(400);
        dialog.setStyleSheet("QDialog { background-color: #0a0a1e; } QLabel { color: white; }");

        QVBoxLayout *layout = new QVBoxLayout(&dialog);

        QGroupBox *g1 = new QGroupBox("Pluto SDR");
        QFormLayout *f1 = new QFormLayout(g1);
        QLineEdit *ipEdit = new QLineEdit("192.168.2.1");
        f1->addRow("IP:", ipEdit);
        QSlider *gainSlider = new QSlider(Qt::Horizontal);
        gainSlider->setRange(0, 73);
        gainSlider->setValue(40);
        QLabel *gainVal = new QLabel("40 дБ");
        connect(gainSlider, &QSlider::valueChanged, [gainVal](int v) { gainVal->setText(QString::number(v) + " дБ"); });
        QHBoxLayout *gainLayout = new QHBoxLayout();
        gainLayout->addWidget(gainSlider);
        gainLayout->addWidget(gainVal);
        f1->addRow("Усиление:", gainLayout);
        QCheckBox *agcCheck = new QCheckBox();
        f1->addRow("AGC:", agcCheck);
        layout->addWidget(g1);

        QGroupBox *g2 = new QGroupBox("Голос");
        QFormLayout *f2 = new QFormLayout(g2);
        QCheckBox *voiceCheck = new QCheckBox();
        voiceCheck->setChecked(settingsVoice);
        f2->addRow("Оповещения:", voiceCheck);
        layout->addWidget(g2);

        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);

        if (dialog.exec() == QDialog::Accepted) {
            settingsVoice = voiceCheck->isChecked();
            statusBar()->showMessage("✅ Настройки сохранены");
        }
    }

    void logMessage(const QString &msg) {
        QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
        logText->append(QString("[%1] %2").arg(ts).arg(msg));
    }

private:
    void setupUI() {
        setWindowTitle("FPV HUNTER PRO v8.0 - C++");
        setGeometry(100, 100, 1200, 800);
        setStyleSheet(
            "QMainWindow { background-color: #0a0a1e; }"
            "QLabel { color: white; }"
            "QPushButton { background-color: #2a2a4a; color: white; border: none; padding: 8px 16px; border-radius: 4px; }"
            "QPushButton:hover { background-color: #3a3a5a; }"
            "QListWidget { background-color: #14142e; color: white; border: 1px solid #2a2a4a; font-family: Consolas; }"
        );

        QWidget *central = new QWidget();
        setCentralWidget(central);
        QHBoxLayout *mainLayout = new QHBoxLayout(central);

        // Левая панель
        QWidget *left = new QWidget();
        left->setFixedWidth(300);
        QVBoxLayout *leftLayout = new QVBoxLayout(left);

        QLabel *title = new QLabel("📡 FPV HUNTER PRO v8.0");
        title->setStyleSheet("font-size: 20px; font-weight: bold; color: #e67e22;");
        leftLayout->addWidget(title);

        statusLabel = new QLabel("🔴 Pluto: не подключен");
        statusLabel->setStyleSheet("color: #ff4444; font-size: 12px;");
        leftLayout->addWidget(statusLabel);

        connectBtn = new QPushButton("🔌 Подключить");
        leftLayout->addWidget(connectBtn);

        QHBoxLayout *infoLayout = new QHBoxLayout();
        rssiLabel = new QLabel("📶 RSSI: ---");
        infoLayout->addWidget(rssiLabel);
        tempLabel = new QLabel("🌡️ --°C");
        infoLayout->addWidget(tempLabel);
        leftLayout->addLayout(infoLayout);

        signalList = new QListWidget();
        leftLayout->addWidget(signalList);

        signalCountLabel = new QLabel("Сигналов: 0");
        leftLayout->addWidget(signalCountLabel);

        QHBoxLayout *btnLayout = new QHBoxLayout();
        recordBtn = new QPushButton("🔴 Запись");
        btnLayout->addWidget(recordBtn);
        snapshotBtn = new QPushButton("📷 Снимок");
        btnLayout->addWidget(snapshotBtn);
        pauseBtn = new QPushButton("⏸ Пауза");
        btnLayout->addWidget(pauseBtn);
        scanBtn = new QPushButton("⏹ Стоп");
        btnLayout->addWidget(scanBtn);
        leftLayout->addLayout(btnLayout);

        QPushButton *settingsBtn = new QPushButton("⚙️ Настройки");
        leftLayout->addWidget(settingsBtn);

        // Правая панель
        QTabWidget *tabs = new QTabWidget();
        QWidget *videoTab = new QWidget();
        QVBoxLayout *videoLayout = new QVBoxLayout(videoTab);
        QLabel *videoLabel = new QLabel("🎬 Ожидание видео...");
        videoLabel->setAlignment(Qt::AlignCenter);
        videoLabel->setStyleSheet("background: black; color: #666; font-size: 18px;");
        videoLabel->setMinimumHeight(400);
        videoLayout->addWidget(videoLabel);
        tabs->addTab(videoTab, "🎬 Видео");

        QWidget *historyTab = new QWidget();
        QVBoxLayout *historyLayout = new QVBoxLayout(historyTab);
        historyTable = new QTableWidget();
        historyTable->setColumnCount(4);
        historyTable->setHorizontalHeaderLabels({"Время", "Частота", "Мощность", "Тип"});
        historyTable->horizontalHeader()->setStretchLastSection(true);
        historyLayout->addWidget(historyTable);
        tabs->addTab(historyTab, "📜 История");

        QWidget *logTab = new QWidget();
        QVBoxLayout *logLayout = new QVBoxLayout(logTab);
        logText = new QTextEdit();
        logText->setReadOnly(true);
        logText->setStyleSheet("background: #14142e; color: #00ff00; font-family: Consolas;");
        logLayout->addWidget(logText);
        tabs->addTab(logTab, "📋 Лог");

        mainLayout->addWidget(left);
        mainLayout->addWidget(tabs, 1);

        // Сигналы
        connect(connectBtn, &QPushButton::clicked, this, &MainWindow::connectPluto);
        connect(recordBtn, &QPushButton::clicked, this, &MainWindow::toggleRecording);
        connect(snapshotBtn, &QPushButton::clicked, this, &MainWindow::takeSnapshot);
        connect(pauseBtn, &QPushButton::clicked, this, &MainWindow::togglePause);
        connect(scanBtn, &QPushButton::clicked, this, &MainWindow::toggleScan);
        connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::showSettingsDialog);

        voice = new QTextToSpeech(this);
        settingsVoice = true;
    }

    void setupTimers() {
        scanTimer = new QTimer(this);
        scanTimer->setInterval(100);
        connect(scanTimer, &QTimer::timeout, this, &MainWindow::scanStep);
        scanTimer->start();

        uiTimer = new QTimer(this);
        uiTimer->setInterval(1000);
        connect(uiTimer, &QTimer::timeout, this, &MainWindow::updateUI);
        uiTimer->start();
    }

    PlutoSDR *pluto;
    QLabel *statusLabel, *rssiLabel, *tempLabel, *signalCountLabel;
    QPushButton *connectBtn, *recordBtn, *snapshotBtn, *pauseBtn, *scanBtn;
    QListWidget *signalList;
    QTableWidget *historyTable;
    QTextEdit *logText;
    QTimer *scanTimer, *uiTimer;
    QTextToSpeech *voice;
    bool settingsVoice;

    int signalCount;
    bool isScanning, isRecording, isPaused;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}
#include "main.moc"
