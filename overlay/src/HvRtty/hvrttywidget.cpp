// MSHV RTTY integration - Copyright 2026
// GPL-compatible derived integration for MSHV.
#include "hvrttywidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDateTime>
#include <QFontDatabase>
#include <QScrollBar>
#include <QRegExp>
#include <QMutexLocker>
#include <cmath>
#include <algorithm>

HvRttyWidget::HvRttyWidget(QWidget *parent)
    : QDialog(parent), active_(false), mainDecoder_(0), pendingPeak_(0),
      audioTimer_(0), lastAudioMs_(-1)
{
    setWindowTitle("WaveStation RTTY");
    resize(920, 540);
    setModal(false);

    baud_ = new QDoubleSpinBox(this);
    baud_->setRange(20.0, 100.0); baud_->setDecimals(2); baud_->setSingleStep(0.01); baud_->setValue(45.45);
    baud_->setSuffix(" baud");
    mark_ = new QSpinBox(this);
    mark_->setRange(300, 3200); mark_->setValue(2125); mark_->setSuffix(" Hz");
    shift_ = new QSpinBox(this);
    shift_->setRange(50, 1000); shift_->setValue(170); shift_->setSuffix(" Hz shift");
    reverse_ = new QCheckBox("Reverse", this);
    multi_ = new QCheckBox("Multi decode", this);
    multi_->setToolTip("Parallel decoder bank across the audio passband. Leave off on slow PCs.");
    spaceLabel_ = new QLabel(this);
    stateLabel_ = new QLabel("RX", this);
    audioLevelLabel_ = new QLabel("Audio: waiting", this);
    audioLevelLabel_->setMinimumWidth(125);

    QHBoxLayout *cfg = new QHBoxLayout;
    cfg->addWidget(new QLabel("Mark:", this)); cfg->addWidget(mark_);
    cfg->addWidget(new QLabel("Shift:", this)); cfg->addWidget(shift_);
    cfg->addWidget(baud_); cfg->addWidget(reverse_); cfg->addWidget(multi_);
    cfg->addWidget(spaceLabel_); cfg->addStretch();
    cfg->addWidget(audioLevelLabel_); cfg->addWidget(stateLabel_);

    rxText_ = new QTextEdit(this);
    rxText_->setReadOnly(true);
    rxText_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    rxText_->setLineWrapMode(QTextEdit::WidgetWidth);

    txEdit_ = new QLineEdit(this);
    txEdit_->setPlaceholderText("RTTY text to transmit...");
    txEdit_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    sendBtn_ = new QPushButton("SEND", this);
    abortBtn_ = new QPushButton("ABORT", this);
    QPushButton *clear = new QPushButton("Clear RX", this);
    QPushButton *cq = new QPushButton("F1 CQ", this);
    QPushButton *rst = new QPushButton("F2 RST", this);
    QPushButton *info = new QPushButton("F3 INFO", this);
    QPushButton *roger = new QPushButton("F4 R", this);
    QPushButton *bye = new QPushButton("F5 73", this);

    cq->setToolTip("General RTTY CQ - non contest");
    rst->setToolTip("Send a normal 599 report");
    info->setToolTip("Normal QSO information / copy check");
    roger->setToolTip("Acknowledge good copy");
    bye->setToolTip("Close the QSO with 73");

    QHBoxLayout *mac = new QHBoxLayout;
    mac->addWidget(cq); mac->addWidget(rst); mac->addWidget(info); mac->addWidget(roger); mac->addWidget(bye);
    mac->addStretch(); mac->addWidget(clear);
    QHBoxLayout *tx = new QHBoxLayout;
    tx->addWidget(txEdit_, 1); tx->addWidget(sendBtn_); tx->addWidget(abortBtn_);

    QVBoxLayout *v = new QVBoxLayout(this);
    v->addLayout(cfg); v->addWidget(rxText_, 1); v->addLayout(mac); v->addLayout(tx);
    setLayout(v);

    connect(baud_, SIGNAL(valueChanged(double)), this, SLOT(RebuildDecoders()));
    connect(mark_, SIGNAL(valueChanged(int)), this, SLOT(RebuildDecoders()));
    connect(shift_, SIGNAL(valueChanged(int)), this, SLOT(RebuildDecoders()));
    connect(reverse_, SIGNAL(toggled(bool)), this, SLOT(RebuildDecoders()));
    connect(multi_, SIGNAL(toggled(bool)), this, SLOT(RebuildDecoders()));
    connect(sendBtn_, SIGNAL(clicked()), this, SLOT(SendClicked()));
    connect(abortBtn_, SIGNAL(clicked()), this, SLOT(AbortClicked()));
    connect(clear, SIGNAL(clicked()), this, SLOT(ClearClicked()));
    connect(cq, SIGNAL(clicked()), this, SLOT(MacroCq()));
    connect(rst, SIGNAL(clicked()), this, SLOT(MacroReport()));
    connect(info, SIGNAL(clicked()), this, SLOT(MacroInfo()));
    connect(roger, SIGNAL(clicked()), this, SLOT(MacroRoger()));
    connect(bye, SIGNAL(clicked()), this, SLOT(Macro73()));
    connect(txEdit_, SIGNAL(returnPressed()), this, SLOT(SendClicked()));

    audioClock_.start();
    audioTimer_ = new QTimer(this);
    audioTimer_->setInterval(40);
    connect(audioTimer_, SIGNAL(timeout()), this, SLOT(ProcessPendingAudio()));
    audioTimer_->start();

    RebuildDecoders();
    hide();
}

HvRttyWidget::~HvRttyWidget() { DestroyDecoders(); }

QString HvRttyWidget::StationCall() const
{
    QString c = myCall_.trimmed().toUpper();
    return c.isEmpty() ? QString("MYCALL") : c;
}

void HvRttyWidget::SetStationCall(QString call)
{
    myCall_ = call.trimmed().toUpper();
}

mshv_rtty::Config HvRttyWidget::CurrentConfig(double markHz) const
{
    mshv_rtty::Config c;
    c.sampleRate = 12000.0;
    c.baud = baud_->value();
    c.markHz = markHz;
    c.spaceHz = reverse_->isChecked() ? markHz - shift_->value() : markHz + shift_->value();
    c.stopBits = 1.5;
    return c;
}

void HvRttyWidget::DestroyDecoders()
{
    delete mainDecoder_; mainDecoder_ = 0;
    for (int i=0;i<lanes_.size();++i) { delete lanes_[i]->decoder; delete lanes_[i]; }
    lanes_.clear();
}

void HvRttyWidget::RebuildDecoders()
{
    DestroyDecoders();
    mainDecoder_ = new mshv_rtty::Decoder(CurrentConfig(mark_->value()));
    mainBuffer_.clear();
    int space = reverse_->isChecked() ? mark_->value()-shift_->value() : mark_->value()+shift_->value();
    spaceLabel_->setText(QString("Space: %1 Hz").arg(space));
    if (active_) emit TuneRequested(mark_->value());

    if (multi_->isChecked()) {
        for (int f=450; f<=2850; f+=85) {
            int sp = reverse_->isChecked() ? f-shift_->value() : f+shift_->value();
            if (sp < 250 || sp > 3300) continue;
            Lane *l = new Lane;
            l->mark = f;
            l->decoder = new mshv_rtty::Decoder(CurrentConfig(f));
            lanes_.append(l);
        }
    }
}

QString HvRttyWidget::Stamp(double markHz, bool multi) const
{
    return QString("[%1] %2%3Hz ")
        .arg(QDateTime::currentDateTimeUtc().toString("hh:mm:ss"))
        .arg(multi ? "M:" : "")
        .arg((int)markHz);
}

void HvRttyWidget::Flush(QString &buffer, double markHz, bool multi)
{
    QString s = buffer;
    s.replace('\r', ' '); s = s.trimmed();
    buffer.clear();
    if (s.length() < 2) return;
    rxText_->append(Stamp(markHz,multi) + s);
    rxText_->verticalScrollBar()->setValue(rxText_->verticalScrollBar()->maximum());
}

void HvRttyWidget::Consume(QString &buffer, const std::string &text, double markHz, bool multi)
{
    for (size_t i=0;i<text.size();++i) {
        char c=text[i];
        if (c=='\n' || c=='\r') {
            if (!buffer.trimmed().isEmpty()) Flush(buffer,markHz,multi);
        } else if ((unsigned char)c>=32 && (unsigned char)c<127) {
            buffer.append(QChar(c));
            if (buffer.length()>=96) Flush(buffer,markHz,multi);
        }
    }
}

void HvRttyWidget::FeedAudio(int *samples, int count)
{
    if (!active_ || !samples || count<=0) return;

    // This function is deliberately limited to a bounded memory copy. It is
    // called on MSHV's live audio path, therefore running trig/DSP or touching
    // Qt widgets here can stall the original waterfall and decoder pipeline.
    QMutexLocker locker(&audioMutex_);

    const int maxQueued = 24000; // at most two seconds at the 12 kHz MSHV stream
    int incoming = std::min(count, maxQueued);
    int start = count - incoming;
    int needDrop = pendingAudio_.size() + incoming - maxQueued;
    if (needDrop > 0) pendingAudio_.remove(0, std::min(needDrop, pendingAudio_.size()));

    for (int i=start; i<count; ++i) {
        int v = samples[i];
        pendingAudio_.append(v);
        int a = (v == -2147483647-1) ? 2147483647 : std::abs(v);
        if (a > pendingPeak_) pendingPeak_ = a;
    }
    lastAudioMs_ = audioClock_.elapsed();
}

void HvRttyWidget::ProcessPendingAudio()
{
    if (!active_) return;

    QVector<int> audio;
    int peak = 0;
    qint64 last = -1;
    {
        QMutexLocker locker(&audioMutex_);
        if (!pendingAudio_.isEmpty()) audio.swap(pendingAudio_);
        peak = pendingPeak_;
        pendingPeak_ = 0;
        last = lastAudioMs_;
    }

    if (!audio.isEmpty() && mainDecoder_) {
        std::string s = mainDecoder_->push(audio.constData(), audio.size());
        if (!s.empty()) Consume(mainBuffer_,s,mark_->value(),false);

        if (multi_->isChecked()) {
            for (int i=0;i<lanes_.size();++i) {
                Lane *l=lanes_[i];
                std::string m=l->decoder->push(audio.constData(),audio.size());
                if (!m.empty()) Consume(l->buffer,m,l->mark,true);
            }
        }
    }

    if (peak > 0) {
        const double full = 8388607.0;
        double db = 20.0 * std::log10(std::max(1.0, (double)peak) / full);
        if (db > 0.0) db = 0.0;
        if (db < -99.0) db = -99.0;
        audioLevelLabel_->setText(QString("Audio: %1 dBFS").arg(db,0,'f',1));
    } else if (last < 0 || audioClock_.elapsed() - last > 800) {
        audioLevelLabel_->setText("Audio: NO INPUT");
    }
}

void HvRttyWidget::SetActive(bool on)
{
    active_=on;
    if (on) {
        {
            QMutexLocker locker(&audioMutex_);
            pendingAudio_.clear(); pendingPeak_=0; lastAudioMs_=-1;
        }
        audioLevelLabel_->setText("Audio: waiting");
        show(); raise(); activateWindow(); stateLabel_->setText("RX / RTTY ACTIVE");
    }
    else {
        stateLabel_->setText("RX"); hide();
        QMutexLocker locker(&audioMutex_);
        pendingAudio_.clear(); pendingPeak_=0; lastAudioMs_=-1;
    }
}

void HvRttyWidget::SetMarkFrequency(double hz)
{
    if (hz < mark_->minimum() || hz > mark_->maximum()) return;
    if (std::abs(mark_->value()-hz) >= 1.0) mark_->setValue((int)std::floor(hz+0.5));
}

void HvRttyWidget::SendClicked()
{
    QString t=txEdit_->text().trimmed();
    if (t.isEmpty()) return;
    stateLabel_->setText("TX"); sendBtn_->setEnabled(false);
    emit SendRequested(t);
}
void HvRttyWidget::AbortClicked() { emit AbortRequested(); SetTxFinished(); }
void HvRttyWidget::SetTxFinished() { stateLabel_->setText("RX / RTTY ACTIVE"); sendBtn_->setEnabled(true); }
void HvRttyWidget::ClearClicked() { rxText_->clear(); mainBuffer_.clear(); }

void HvRttyWidget::MacroCq()
{
    QString c=StationCall();
    txEdit_->setText(QString("CQ CQ CQ DE %1 %1 %1 K").arg(c));
}
void HvRttyWidget::MacroReport()
{
    QString c=StationCall();
    txEdit_->setText(QString("UR RST 599 599 599 HW COPY? DE %1 K").arg(c));
}
void HvRttyWidget::MacroInfo()
{
    QString c=StationCall();
    txEdit_->setText(QString("TNX FER CALL. RTTY 45 BAUD 170 SHIFT. HW COPY? DE %1 K").arg(c));
}
void HvRttyWidget::MacroRoger()
{
    QString c=StationCall();
    txEdit_->setText(QString("R R ALL COPY. TNX FER INFO. DE %1 K").arg(c));
}
void HvRttyWidget::Macro73()
{
    QString c=StationCall();
    txEdit_->setText(QString("TNX FER QSO. 73 73 DE %1 SK").arg(c));
}
