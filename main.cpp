#include <QtWidgets>
#include <QMediaPlayer>
#include <QPushButton>
#include <QtMultimedia>



class WaveformWidget : public QWidget {

public:
    explicit WaveformWidget(QWidget *parent = nullptr) : QWidget(parent), m_zoomLevel(1) {}

    void setPlaybackPosition(qint64 position) {
        m_playbackPosition = position * width() / m_duration;
        update();
    }

    void setDuration(qint64 duration) {
        m_duration = duration;
    }

    void loadAudioFile(const QString& filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Unable to open file:" << filePath;
            return;
        }
        qDebug() << "Is Visible:" << isVisible();
        qDebug() << "Size:" << size();
        m_data = normalizeSamples(getSamples(file));
        qDebug() << "Loaded" << m_data.size() << "samples";
        update();
    }

    void setZoomLevel(int zoom) {
        m_zoomLevel = zoom;
        qDebug() << "Zoom level:" << zoom;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        // Check if an audio file has been loaded
        if (m_data.isEmpty()) {
            return;
        }

        QPainter p(this);
        p.fillRect(rect(), Qt::black);
        p.setPen(QPen(Qt::red, 1));

        int numSamples = m_data.size();
        int width = size().width();
        int height = size().height();

        // Check if width is zero
        if (width == 0) {
            return;
        }

        // Calculate the number of samples per pixel
        int samplesPerPixel = numSamples / width * m_zoomLevel;

        // Draw a line for each pixel
        for (int x = 0; x < width; ++x) {
            // Calculate the start and end of the sample range for this pixel
            int start = x * samplesPerPixel;
            int end = std::min((x + 1) * samplesPerPixel, numSamples);

            // Find the minimum and maximum sample in this range
            auto minmax = std::minmax_element(m_data.begin() + start, m_data.begin() + end);

            // Map the minimum and maximum sample to the height of the widget
            int yMin = ((1.0f - *minmax.first) / 2.0f) * height;
            int yMax = ((1.0f - *minmax.second) / 2.0f) * height;

            // Draw a line from the minimum to the maximum sample
            p.drawLine(x, yMin, x, yMax);
        }

        p.setPen(QPen(Qt::green, 2));
        p.drawLine(m_playbackPosition, 0, m_playbackPosition, QWidget::height());

    }




private:
    QVector<float> m_data;
    int m_zoomLevel;
    int m_playbackPosition = 0;
    qint64 m_duration = 0;

    QVector<qint16> getSamples(QFile& file) {
        QVector<qint16> samples;

        QDataStream in(&file);
        qDebug() << "Reading" << file.size() << "bytes";
        while (!in.atEnd()) {
            qint16 sample;
            in.readRawData((char*)&sample, sizeof(sample));
            samples.append(sample);
        }
        qDebug() << "Read" << samples.size() << "samples";

        return samples;
    }

    QVector<float> normalizeSamples(const QVector<qint16>& samples) {
        QVector<float> output;

        // Find maximum absolute value
        float maxValue = 0.0f;
        for (auto sample : samples) {
            if (abs(sample) > maxValue) {
                maxValue = abs(sample);
            }
        }

        // Normalize samples
        for (auto sample : samples) {
            output.push_back(static_cast<float>(sample) / maxValue);
        }

        return output;
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Main window and layout
    QWidget window;
    QVBoxLayout mainLayout(&window);

    // Horizontal layout for file explorer and file section
    QHBoxLayout fileLayout;
    mainLayout.addLayout(&fileLayout);

    // File system navigation
    QFileSystemModel* dirModel = new QFileSystemModel();
    dirModel->setRootPath(QDir::homePath());

    QTreeView* treeView = new QTreeView();
    treeView->setModel(dirModel);
    fileLayout.addWidget(treeView);

    QTableView* tableView = new QTableView();
    QFileSystemModel* fileModel = new QFileSystemModel();
    tableView->setModel(fileModel);
    fileLayout.addWidget(tableView);

    // Waveform widget and zoom controls
    WaveformWidget* waveform = new WaveformWidget();
    waveform->setMinimumSize(100, 100);
    mainLayout.addWidget(waveform);

    // Media player
    QMediaPlayer* player = new QMediaPlayer();

    // Connect player signals to waveform slots
    QObject::connect(player, &QMediaPlayer::positionChanged, waveform, &WaveformWidget::setPlaybackPosition);
    QObject::connect(player, &QMediaPlayer::durationChanged, waveform, &WaveformWidget::setDuration);

    // Playback controls
    QHBoxLayout controlLayout;
    mainLayout.addLayout(&controlLayout);

    QPushButton* playButton = new QPushButton("Play");
    controlLayout.addWidget(playButton);
    QObject::connect(playButton, &QPushButton::clicked, player, &QMediaPlayer::play);

    QPushButton* pauseButton = new QPushButton("Pause");
    controlLayout.addWidget(pauseButton);
    QObject::connect(pauseButton, &QPushButton::clicked, player, &QMediaPlayer::pause);

    QPushButton* stopButton = new QPushButton("Stop");
    controlLayout.addWidget(stopButton);
    QObject::connect(stopButton, &QPushButton::clicked, player, &QMediaPlayer::stop);

    // Connect tree view selection to populate table view
    QObject::connect(treeView->selectionModel(),
                     &QItemSelectionModel::currentChanged,
                     [=](const QModelIndex &index) {
                         QString path = dirModel->fileInfo(index).absoluteFilePath();

                         if (dirModel->fileInfo(index).isDir()) {
                             QStringList filters;
                             filters << "*.wav" << "*.mp3" << "*.flac";
                             fileModel->setNameFilters(filters);

                             fileModel->setRootPath(path);
                             tableView->setRootIndex(fileModel->index(path));
                         }
                     });

    // Connect file selection to load audio and update waveform
    QObject::connect(tableView->selectionModel(),
                     &QItemSelectionModel::currentRowChanged,
                     [=](const QModelIndex &index) {
                         QString filePath = fileModel->data(index, QFileSystemModel::FilePathRole).toString();
                         player->setSource(QUrl::fromLocalFile(filePath));

                         waveform->loadAudioFile(filePath);
                     });


    window.show();
    waveform->show();
    return app.exec();
}
