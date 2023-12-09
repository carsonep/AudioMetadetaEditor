#include <QtWidgets>

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

void normalizeSamples(const QVector<qint16>& input,

                      QVector<float>& output) {

    // Normalize code

    std::transform(input.begin(), input.end(),

                   std::back_inserter(output),

                   [](qint16 s) { return s / 32768.0f; }

                   );

}

class WaveformWidget : public QWidget {

public:

    void loadAudioFile(const QString &filePath) {

        // Open audio file

        QFile file(filePath);

        file.open(QIODevice::ReadOnly);

        // Read samples into buffer

        QVector<qint16> samples = getSamples(file);

        // Normalize to -1 to 1

        QVector<float> normalized;

        normalizeSamples(samples, normalized);

        // Trigger paint with new data

        m_data = normalized;
        for (int i = 0; i < 1000; ++i) {
            m_data.append(sin(i * 2 * M_PI / 100.0));
        }

        update();

    }

protected:

    void paintEvent(QPaintEvent*) override {

        QPainter p(this);

        // Draw waveform
        p.setPen(Qt::blue);
        int numSamples = m_data.size();
        int width = rect().width();
        int step = numSamples > 0 ? width / (numSamples - 1) : 1;
        for (int x = 0; x < width; x += step) {
            int sampleIdx = x * numSamples / width;
            float sample = m_data[sampleIdx];
            int y = rect().height()/2 + sample * rect().height()/2;
            p.drawLine(x, y, x+step, y);
        }

    }

private:

    QVector<float> m_data;

};

int main(int argc, char *argv[]) {

    QApplication app(argc, argv);

    QWidget window;
    QHBoxLayout layout(&window);

    QFileSystemModel* dirModel = new QFileSystemModel();
    dirModel->setRootPath(QDir::homePath());

    QTreeView* treeView = new QTreeView();
    treeView->setModel(dirModel);
    layout.addWidget(treeView);

    QTableView* tableView = new QTableView();

    // Will hold file contents for current dir
    QFileSystemModel* fileModel = new QFileSystemModel();
    tableView->setModel(fileModel);

    layout.addWidget(tableView);

    // Connect tree view selection to populate table view
    QObject::connect(treeView->selectionModel(),
                     &QItemSelectionModel::currentChanged,
                     [=](const QModelIndex &index){
                         if(dirModel->fileInfo(index).isDir()) {
                             fileModel->setRootPath(dirModel->fileInfo(index).absoluteFilePath());
                         }
                     });

    WaveformWidget* waveform = new WaveformWidget();
    layout.addWidget(waveform);

    // Get file path from table view index
    QObject::connect(tableView->selectionModel(),
                     &QItemSelectionModel::currentRowChanged,
                     [=](const QModelIndex &index) {
                         QString filePath = fileModel->data(index, QFileSystemModel::FilePathRole).toString();
                         waveform->loadAudioFile(filePath);
                     });

    window.show();
    return app.exec();
}
