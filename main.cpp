#include <QtWidgets>
#include <QMediaPlayer>
#include <QPushButton>
#include <QtMultimedia>
#include <QAudioOutput>
#include <sndfile.h>
#include <QFileSystemModel>

#include <taglib/tag.h>
#include <taglib/fileref.h>

class MetadataFileModel : public QFileSystemModel {
public:
    int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        return 4;
    }
    QVariant data(const QModelIndex &index, int role) const override {

        if(role == Qt::DisplayRole && index.column() >= 1) {
            QString path = fileInfo(index).absoluteFilePath();
            SF_INFO sfinfo;
            SNDFILE* sndfile = sf_open(path.toUtf8(), SFM_READ, &sfinfo);
            if(!sndfile) {
                return QVariant();
            }

            switch(index.column()) {
            case 1:
                return sfinfo.samplerate;
            case 2:
                return sfinfo.channels;
            case 3:
                return QVariant(static_cast<int>(8 * sizeof(float))); // bit depth
            default:
                break;
            }

            sf_close(sndfile);
        }

        return QFileSystemModel::data(index, role);
    }
};



void updateMetadataDisplay(const QModelIndex &index, QTextEdit* display, QFileSystemModel* model) {
    if (!model) return;

    QString filePath = model->fileInfo(index).absoluteFilePath();
    SF_INFO sfinfo;
    SNDFILE* sndfile = sf_open(filePath.toStdString().c_str(), SFM_READ, &sfinfo);

    QString metadataStr;

    if (sndfile != nullptr) {
        metadataStr += QString("Filename: %1\nSample Rate: %2 Hz\nChannels: %3\nFrames: %4\n")
                           .arg(filePath)
                           .arg(sfinfo.samplerate)
                           .arg(sfinfo.channels)
                           .arg(sfinfo.frames);

        metadataStr += QString("Format: %1\n").arg(sfinfo.format);
        metadataStr += QString("Sections: %1\n").arg(sfinfo.sections);
        metadataStr += QString("Seekable: %1\n").arg(sfinfo.seekable);

        sf_close(sndfile);
    } else {
        metadataStr += "Unable to read file metadata using libsndfile.\n";
    }
    QByteArray utf8File = filePath.toUtf8();
    const char* fileName = utf8File.constData();

    TagLib::FileRef f(fileName);
    // Using TagLib for additional metadata


    if (!f.isNull() && f.tag()) {
        TagLib::Tag* tag = f.tag();
        metadataStr += QString("Title: %1\n").arg(QString::fromStdWString(tag->title().toWString()));
        metadataStr += QString("Artist: %1\n").arg(QString::fromStdWString(tag->artist().toWString()));
        metadataStr += QString("Album: %1\n").arg(QString::fromStdWString(tag->album().toWString()));
        metadataStr += QString("Year: %1\n").arg(tag->year());
        metadataStr += QString("Track: %1\n").arg(tag->track());
        metadataStr += QString("Genre: %1\n").arg(QString::fromStdWString(tag->genre().toWString()));
    } else {
        metadataStr += "Additional metadata not available or unsupported file format.";
    }

    display->setText(metadataStr);
}


class PlaybackLineWidget : public QWidget {
public:
    explicit PlaybackLineWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents); // Ignore mouse events
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setPen(QPen(Qt::white, 1)); // Set the color and width of the playback line
        painter.drawLine(rect().width() / 2, 0, rect().width() / 2, rect().height());
    }


};




class WaveformWidget : public QWidget {
PlaybackLineWidget *playbackLine;
public:
explicit WaveformWidget(QWidget *parent = nullptr) : QWidget(parent), m_zoomLevel(1) {
    playbackLine = new PlaybackLineWidget(this);
    playbackLine->resize(3, height()); // Initial size, 3 pixels wide
}

void setPlaybackPosition(qint64 position) {
    m_playbackPosition = position * width() / m_duration;
    playbackLine->move(m_playbackPosition - playbackLine->width() / 2, 0); // Center the line at the position
    playbackLine->update(); // Redraw only the playback line
}

    void setDuration(qint64 duration) {
        m_duration = duration;
    }

    void loadAudioFile(const QString& filePath) {
        SF_INFO sfinfo;
        SNDFILE *sndfile = sf_open(filePath.toStdString().c_str(), SFM_READ, &sfinfo);
        if (sndfile == nullptr) {
            qDebug() << "Error opening sound file:" << sf_strerror(sndfile);
            return;
        }

        // Store sample rate, bit depth, and number of channels
        m_sampleRate = sfinfo.samplerate;
        m_bitDepth = 8 * sizeof(float); // libsndfile converts to float
        m_channels = sfinfo.channels;

        // Reading the file into buffer
        std::vector<float> buffer(sfinfo.frames * sfinfo.channels);
        sf_read_float(sndfile, buffer.data(), buffer.size());

        // Convert buffer to QVector
        m_data.clear();
        m_data.reserve(buffer.size());
        for (auto &sample : buffer) {
            m_data.push_back(sample);
        }

        sf_close(sndfile);
        update();
    }


    void setZoomLevel(int zoom) {
        m_zoomLevel = zoom;
        qDebug() << "Zoom level:" << zoom;
        update();
    }



protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        playbackLine->resize(3, height()); // Adjust the height of the playback line
        setPlaybackPosition(m_playbackPosition); // Recalculate the position of the playback line
    }
    void paintEvent(QPaintEvent*) override {
        if (m_data.isEmpty() || m_channels < 1) {
            return;
        }

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);


        // Set the pen for drawing the waveform
        QPen pen;
        pen.setWidth(1);

        int numSamples = m_data.size() / m_channels;
        int width = size().width();
        int height = size().height() / m_channels;  // Split height by the number of channels

        // Iterate over each channel
        for (int ch = 0; ch < m_channels; ++ch) {
            QPainterPath waveformPath; // Path for the waveform fill
            QPainterPath outlinePath;  // Path for the waveform outline

            // Set initial positions for the paths
            waveformPath.moveTo(0, this->height() / 2);
            outlinePath.moveTo(0, this->height() / 2);


            pen.setColor(ch == 0 ? Qt::red : Qt::green);  // Example: Red for first channel, green for second
            p.setPen(pen);

            // Calculate the number of samples per pixel
            int samplesPerPixel = numSamples / width;

            // Draw the waveform fill
            QColor fillColor = Qt::darkGray; // Example: Dark gray for waveform fill
            p.fillPath(waveformPath, fillColor);

            // Draw the waveform outline
            QPen outlinePen(Qt::white, 1); // Example: White outline
            p.strokePath(outlinePath, outlinePen);

            // Draw a line for each pixel
            for (int x = 0; x < width; ++x) {
                int start = x * samplesPerPixel * m_channels + ch;
                int end = std::min((x + 1) * samplesPerPixel * m_channels + ch, static_cast<int>(m_data.size()) - m_channels);


                // Find the minimum and maximum sample in this range
                float minSample = 1.0f;
                float maxSample = -1.0f;
                for (int i = start; i < end; i += m_channels) {
                    minSample = std::min(minSample, m_data[i]);
                    maxSample = std::max(maxSample, m_data[i]);
                }

                // Map the minimum and maximum sample to the height of the widget
                int yMin = height * (1 - minSample) / 2 + ch * height;
                int yMax = height * (1 - maxSample) / 2 + ch * height;

                // Draw a line from the minimum to the maximum sample
                p.drawLine(x, yMin, x, yMax);

                waveformPath.lineTo(x, yMax);
                waveformPath.lineTo(x, yMin);
                outlinePath.lineTo(x, yMax);
                outlinePath.lineTo(x, yMin);
            }

            waveformPath.lineTo(width, this->height() / 2); // Close the waveform path
            outlinePath.lineTo(width, this->height() / 2);  // Close the outline path

        }


    }




private:
    QVector<float> m_data; // This will now store interleaved samples for all channels
    int m_channels = 0;
    int m_zoomLevel;
    int m_playbackPosition = 0;
    qint64 m_duration = 0;
    int m_sampleRate = 0;  // New member for sample rate
    int m_bitDepth = 0;    // New member for bit depth

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
};



int main(int argc, char *argv[]) {


    QApplication app(argc, argv);

    // Main window and layout
    QWidget window;
    QVBoxLayout mainLayout(&window);

    // Horizontal layout for file explorer and file section
    QHBoxLayout fileLayout;
    mainLayout.addLayout(&fileLayout);

    QFileSystemModel* dirModel = new QFileSystemModel();
    dirModel->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs); // Show only directories
    dirModel->setRootPath(QDir::homePath());

    QTreeView* treeView = new QTreeView();
    treeView->setModel(dirModel);
    fileLayout.addWidget(treeView);

    // File system navigation for audio files
    MetadataFileModel* fileModel = new MetadataFileModel();
    fileModel->setFilter(QDir::NoDotAndDotDot | QDir::Files); // Show only files
    QStringList audioFilters = {"*.wav", "*.mp3", "*.flac"};
    fileModel->setNameFilters(audioFilters); // Filter for audio files
    fileModel->setRootPath(QDir::homePath()); // Optionally set an initial path

    QTableView* tableView = new QTableView();
    tableView->setModel(fileModel);
    fileLayout.addWidget(tableView);
    // Waveform widget and zoom controls
    WaveformWidget* waveform = new WaveformWidget();
    waveform->setMinimumSize(200, 200);
    mainLayout.addWidget(waveform);
    qputenv("QT_AUDIO_BACKEND", "coreaudio");
    // Media player
    QMediaPlayer* player = new QMediaPlayer();

    QAudioOutput* audioOutput = new QAudioOutput;  // Create QAudioOutput
    player->setAudioOutput(audioOutput);            // Set audio output for the player
    /*audioOutput->setVolume(0.5)*/;



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

    QTextEdit* metadataDisplay = new QTextEdit();
    metadataDisplay->setReadOnly(true);  // Make it read-only
    fileLayout.addWidget(metadataDisplay);  // Add it to the layout
    QSlider* volumeSlider = new QSlider(Qt::Horizontal);
    volumeSlider->setRange(0, 100); // 0 to 100 percent
    controlLayout.addWidget(volumeSlider);

    QObject::connect(volumeSlider, &QSlider::valueChanged,
                     [=](int value) {
                         qreal linearVolume = value / 100.0; // 0.0 to 1.0
                         audioOutput->setVolume(linearVolume);
                     });

    volumeSlider->setValue(audioOutput->volume() * 100);
    ;
    // Connect tree view selection to populate table view
    QObject::connect(treeView->selectionModel(), &QItemSelectionModel::currentChanged,
                     [=](const QModelIndex &index) {
                         QString path = dirModel->fileInfo(index).absoluteFilePath();
                         fileModel->setRootPath(path);
                         tableView->setRootIndex(fileModel->index(path));
                     });

    // Connect file selection to load audio and update waveform
    QObject::connect(tableView->selectionModel(),
                     &QItemSelectionModel::currentRowChanged,
                     [=](const QModelIndex &index) {
                         QString filePath = fileModel->data(index, QFileSystemModel::FilePathRole).toString();
                         player->setSource(QUrl::fromLocalFile(filePath));

                         waveform->loadAudioFile(filePath);
                     });

    QObject::connect(tableView->selectionModel(), &QItemSelectionModel::currentRowChanged,
                     [=](const QModelIndex &index) {
                         updateMetadataDisplay(index, metadataDisplay, fileModel);
                     });



    window.show();
    waveform->show();
    return app.exec();
}
