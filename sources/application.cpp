#include "application.h"
#include "ui_main_window.h"
#include "wave_processor.h"
#include "wavetable.h"
#include <Qsci/qscilexermatlab.h>
#include <Q3DScatter>
#include <QMessageBox>
#include <QFileDialog>
#include <QMenu>
#include <QWidgetAction>
#include <QSlider>
#include <QLabel>
#include <QToolButton>
#include <QToolButton>
#include <QFontDatabase>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <functional>

using namespace QtDataVisualization;

class SliderAction;

struct Application::Impl {
    std::unique_ptr<WaveProcessor> waveProc_;
    Wavetable_s waveTable_;
    Q3DScatter *wavePlot3D_ = nullptr;

    ///
    QTimer *runCodeTimer_ = nullptr;
    QMainWindow *window_ = nullptr;
    std::unique_ptr<Ui::MainWindow> ui_;

    SliderAction *actionSetTableSize_ = nullptr;
    SliderAction *actionSetNumTables_ = nullptr;

    QString lastFilename;

    enum : unsigned {
        minTableSizeLog2 = 6,
        maxTableSizeLog2 = 12,
        defTableSizeLog2 = 11,
        minNumTables = 8,
        maxNumTables = 256,
        defNumTables = 64,
    };

    ///
    void runCode();
    void onWavetableUpdated();
    void showError(const QString &msg);

    ///
    void onOpen();
    void onSave();
    void onSaveAs();
    void doSave(const QString &filename);
    void onExport();
};

///
class SliderAction : public QWidgetAction {
public:
    typedef std::function<QString(int)> TextFunction;

    explicit SliderAction(QObject *parent = nullptr)
        : QWidgetAction(parent)
    {
        widget_ = new QWidget;
        widget_->setLayout(new QVBoxLayout);
        slider_ = new QSlider;
        widget_->layout()->addWidget(slider_);
        label_ = new QLabel;
        label_->setAlignment(Qt::AlignHCenter);
        widget_->layout()->addWidget(label_);
        slider_->setOrientation(Qt::Horizontal);
        setDefaultWidget(widget_);
        connect(slider_, &QSlider::valueChanged, this, [this]() { updateLabel(); });
        updateLabel();
    }

    QSlider *slider() const { return slider_; }
    QLabel *label() const { return label_; }
    void setTextFunction(TextFunction tf) { text_ = std::move(tf); updateLabel(); }

private:
    void updateLabel()
    {
        if (text_)
            label_->setText(text_(slider_->value()));
    }

private:
    QWidget *widget_ = nullptr;
    QSlider *slider_ = nullptr;
    QLabel *label_ = nullptr;
    TextFunction text_;
};

///
Application::Application(int &argc, char *argv[])
    : QApplication(argc, argv)
{
    setApplicationName(PROJECT_NAME);
}

Application::~Application()
{
}

bool Application::init()
{
    Impl *impl = new Impl;
    impl_.reset(impl);

    ///
    WaveProcessor *waveProc = new WaveProcessor;
    impl->waveProc_.reset(waveProc);

    if (!*waveProc) {
        QMessageBox::critical(nullptr, tr("Error"), tr("Could not initialize the Octave interpreter."));
        return false;
    }

    ///
    QMainWindow *window = new QMainWindow;
    impl_->window_ = window;
    Ui::MainWindow *ui = new Ui::MainWindow;
    impl->ui_.reset(ui);

    ui->setupUi(window);

    ///
    QToolButton *docButton = qobject_cast<QToolButton *>(ui->toolBar->widgetForAction(ui->actionDocument));
    QMenu *docMenu = new QMenu(window);
    docButton->setMenu(docMenu);
    docButton->setPopupMode(QToolButton::InstantPopup);

    docMenu->addAction(ui->actionOpen);
    docMenu->addAction(ui->actionSave);
    docMenu->addAction(ui->actionSave_as);
    docMenu->addSeparator();
    docMenu->addAction(ui->actionExport);

    QToolButton *settingsButton = qobject_cast<QToolButton *>(ui->toolBar->widgetForAction(ui->actionSettings));
    QMenu *settingsMenu = new QMenu(window);
    settingsButton->setMenu(settingsMenu);
    settingsButton->setPopupMode(QToolButton::InstantPopup);

    SliderAction *actionSetTableSize = new SliderAction(window);
    impl->actionSetTableSize_ = actionSetTableSize;
    actionSetTableSize->slider()->setMinimumWidth(200);
    actionSetTableSize->slider()->setRange(Impl::minTableSizeLog2, Impl::maxTableSizeLog2);
    actionSetTableSize->slider()->setValue(Impl::defTableSizeLog2);
    actionSetTableSize->setTextFunction([](int v) { return QString("Table size: %0").arg(1 << v); });
    settingsMenu->addAction(actionSetTableSize);

    settingsMenu->addSeparator();

    SliderAction *actionSetNumTables = new SliderAction(window);
    impl->actionSetNumTables_ = actionSetNumTables;
    actionSetNumTables->slider()->setMinimumWidth(200);
    actionSetNumTables->slider()->setRange(Impl::minNumTables, Impl::maxNumTables);
    actionSetNumTables->slider()->setValue(Impl::defNumTables);
    actionSetNumTables->setTextFunction([](int v) { return QString("Table count: %0").arg(v); });
    settingsMenu->addAction(actionSetNumTables);

    ///
    Q3DScatter *wavePlot3D = new Q3DScatter;
    impl->wavePlot3D_ = wavePlot3D;
    ui->frmPlot->layout()->addWidget(QWidget::createWindowContainer(wavePlot3D));

    Q3DTheme *waveTheme3D = wavePlot3D->activeTheme();
    {   QFont font = waveTheme3D->font();
        font.setPointSize(48);
        waveTheme3D->setFont(font);
    }

    Q3DScene *scene3D = wavePlot3D->scene();
    scene3D->activeCamera()->setCameraPreset(Q3DCamera::CameraPresetIsometricLeft);

    QScatterDataProxy *proxy = new QScatterDataProxy;
    QScatter3DSeries *series = new QScatter3DSeries(proxy);
    wavePlot3D->addSeries(series);

    series->setItemLabelFormat(QStringLiteral("@xTitle=@xLabel @zTitle=@zLabel @yTitle=@yLabel"));

    wavePlot3D->axisX()->setTitle("X");
    wavePlot3D->axisY()->setTitle("Wave");
    wavePlot3D->axisZ()->setTitle("Y");
    wavePlot3D->axisX()->setTitleVisible(true);
    wavePlot3D->axisY()->setTitleVisible(true);
    wavePlot3D->axisZ()->setTitleVisible(true);

    ///
    QsciScintilla *txtCode = ui->txtCode;
    txtCode->setMarginLineNumbers(1, true);
    txtCode->setText(
        "% X\t[in]\tarray of sample positions to evaluate [0;1[\n"
        "% Y\t[in]\tfractional position of the subtable [0;1]\n"
        "% wave\t[out]\tarray result of the same length as X\n"
        "wave=sin(2*pi*X) + Y*rand(1, length(X));\n"
        );
    txtCode->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    QsciLexerMatlab *lexer = new QsciLexerMatlab(txtCode);
    lexer->setDefaultFont(txtCode->font());
    txtCode->setLexer(lexer);

    ui->hsplitter->setSizes({INT_MAX, INT_MAX});
    ui->vsplitter->setSizes({int(INT_MAX * 0.75), int(INT_MAX * 0.25)});

    QPlainTextEdit *txtOutput = ui->txtOutput;
    txtOutput->setReadOnly(true);

    window->setWindowTitle(applicationDisplayName());
    window->show();

    ///
    QTimer *runCodeTimer = new QTimer;
    impl->runCodeTimer_ = runCodeTimer;
    runCodeTimer->setSingleShot(true);
    runCodeTimer->setInterval(std::chrono::milliseconds(250));

    connect(runCodeTimer, &QTimer::timeout,
            this, [impl]() { impl->runCode(); });

    connect(ui->txtCode, &QsciScintilla::textChanged,
            this, [runCodeTimer]() { runCodeTimer->start(); });

    connect(actionSetTableSize->slider(), &QSlider::valueChanged,
            this, [impl, runCodeTimer](int v) { runCodeTimer->start(); });
    connect(actionSetNumTables->slider(), &QSlider::valueChanged,
            this, [impl, runCodeTimer](int v) { runCodeTimer->start(); });

    connect(ui->actionOpen, &QAction::triggered, this, [impl]() { impl->onOpen(); });
    connect(ui->actionSave, &QAction::triggered, this, [impl]() { impl->onSave(); });
    connect(ui->actionSave_as, &QAction::triggered, this, [impl]() { impl->onSaveAs(); });
    connect(ui->actionExport, &QAction::triggered, this, [impl]() { impl->onExport(); });

    runCodeTimer->start();

    return true;
}

void Application::Impl::runCode()
{
    const unsigned count = actionSetNumTables_->slider()->value();
    const unsigned frames = 1 << actionSetTableSize_->slider()->value();
    const std::string wavecode = ui_->txtCode->text().toStdString();

    std::string errmsg;
    Wavetable_s wt(waveProc_->process(wavecode, count, frames, &errmsg));

    QPlainTextEdit *txtOutput = ui_->txtOutput;
    if (!wt) {
        showError(QString::fromStdString(errmsg));
    }
    else {
        showError(QString());
        waveTable_ = wt;
        onWavetableUpdated();
    }
}

void Application::Impl::onWavetableUpdated()
{
    const Wavetable &wt = *waveTable_;

    if (wt.count < 1 || wt.frames < 1)
        return;

    const unsigned axisMaxPoints = 64; // too many points lag the graph
    unsigned w_step = std::max(1u, wt.count / axisMaxPoints);
    unsigned f_step = std::max(1u, wt.frames / axisMaxPoints);

    Q3DScatter *plt = wavePlot3D_;
    QScatter3DSeries *series = plt->seriesList().at(0);
    QScatterDataProxy *proxy = series->dataProxy();

    QScatterDataArray *dataArray = new QScatterDataArray;
    dataArray->resize((wt.count / w_step) * (wt.frames / f_step));
    QScatterDataItem *ptrToDataArray = &dataArray->first();

    for (unsigned w_i = 0, w_n = wt.count; w_i < w_n; w_i += w_step) {
        for (unsigned f_i = 0, f_n = wt.frames; f_i < f_n; f_i += f_step) {
            double w = double(w_i) / double(w_n - 1);
            double f = double(f_i) / double(f_n);
            double value = wt.data[w_i * f_n + f_i];
            ptrToDataArray->setPosition(QVector3D(f, value, w));
            ++ptrToDataArray;
        }
    }

    proxy->resetArray(dataArray);
}

void Application::Impl::showError(const QString &msg)
{
    QPlainTextEdit *txtOutput = ui_->txtOutput;
    if (!msg.isEmpty()) {
        txtOutput->setStyleSheet("color: #A40000; background-color: #FFFFBF;");
        txtOutput->setPlainText(msg);
    }
    else {
        txtOutput->setStyleSheet("color: #2E3436; background-color: #FFFFBF;");
        txtOutput->setPlainText("The program has run successfully.");
    }
}

void Application::Impl::onOpen()
{
    QFileDialog dlg(window_, tr("Open file"), QString(), tr("Wavetable source (*.wtf)"));
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setDefaultSuffix("wtf");
    QString filename;
    if (!dlg.exec())
        return;
    filename = dlg.selectedFiles().front();

    QFile file(filename);
    if (!file.open(QFile::ReadOnly)) {
        QMessageBox::warning(window_, tr("Error"), tr("Could not open the file for reading."));
        return;
    }

    QByteArray filedata = file.readAll();
    if (file.error() != QFile::NoError) {
        QMessageBox::warning(window_, tr("Error"), tr("Could not read the file data."));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(filedata);
    if (doc.isNull() || doc["file-type"].toString() != "Wavetable source") {
        QMessageBox::warning(window_, tr("Error"), tr("The file format is incorrect."));
        return;
    }

    actionSetNumTables_->slider()->setValue(doc["table-count"].toInt());
    actionSetTableSize_->slider()->setValue(doc["table-size-log2"].toInt());
    ui_->txtCode->setText(doc["source"].toString());

    runCodeTimer_->start();

    lastFilename = filename;
}

void Application::Impl::onSave()
{
    if (lastFilename.isEmpty())
        onSaveAs();
    else
        doSave(lastFilename);
}

void Application::Impl::onSaveAs()
{
    QFileDialog dlg(window_, tr("Save file"), QString(), tr("Wavetable source (*.wtf)"));
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setDefaultSuffix("wtf");
    QString filename;
    if (!dlg.exec())
        return;
    filename = dlg.selectedFiles().front();

    doSave(filename);
}

void Application::Impl::doSave(const QString &filename)
{
    QJsonObject obj;
    obj["file-type"] = "Wavetable source";
    obj["file-version"] = "1";
    obj["source"] = ui_->txtCode->text();
    obj["table-count"] = qint64(actionSetNumTables_->slider()->value());
    obj["table-size-log2"] = qint64(actionSetTableSize_->slider()->value());
    QJsonDocument doc(obj);

    QFile file(filename);
    if (!file.open(QFile::WriteOnly)) {
        QMessageBox::warning(window_, tr("Error"), tr("Could not open the file for writing."));
        return;
    }

    file.write(doc.toJson());
    file.flush();

    if (file.error() != QFile::NoError) {
        file.remove();
        QMessageBox::warning(window_, tr("Error"), tr("Could not write the file data."));
        return;
    }

    lastFilename = filename;
}

void Application::Impl::onExport()
{
    if (!waveTable_)
        return;

    QFileDialog dlg(window_, tr("Export file"), QString(), tr("WAV file (*.wav)"));
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setDefaultSuffix("wav");
    QString filename;
    if (!dlg.exec())
        return;
    filename = dlg.selectedFiles().front();

    QFile file(filename);
    if (!file.open(QFile::WriteOnly)) {
        QMessageBox::warning(window_, tr("Error"), tr("Could not open the file for writing."));
        return;
    }

    Wavetables::saveToWAVFile(file, *waveTable_, ui_->txtCode->text());

    if (file.error() != QFile::NoError) {
        file.remove();
        QMessageBox::warning(window_, tr("Error"), tr("Could not write the file data."));
        return;
    }
}
