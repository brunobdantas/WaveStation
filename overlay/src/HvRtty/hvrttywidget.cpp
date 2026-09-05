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
#include <cmath>

HvRttyWidget::HvRttyWidget(QWidget *parent)
    : QDialog(parent), active_(false), mainDecoder_(0)
{
    setWindowTitle("WaveStation RTTY");
    resize(840, 520);
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

    QHBoxLayout *cfg = new QHBoxLayout;
    cfg->addWidget(new QLabel("Mark:", this)); cfg->addWidget(mark_);
    cfg->addWidget(new QLabel("Shift:", this)); cfg->addWidget(shift_);
    cfg->addWidget(baud_); cfg->addWidget(reverse_); cfg->addWidget(multi_);
    cfg->addWidget(spaceLabel_); cfg->addStretch(); cfg->addWidget(stateLabel_);

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
    QPushButton *ex = new QPushButton("F2 599 001", this);
    QPushButton *tu = new QPushButton("F3 TU", this);

    QHBoxLayout *mac = new QHBoxLayout;
    mac->addWidget(cq); mac->addWidget(ex); mac->addWidget(tu); mac->addStretch(); mac->addWidget(clear);
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
    connect(ex, SIGNAL(clicked()), this, SLOT(MacroExch()));
    connect(tu, SIGNAL(clicked()), this, SLOT(MacroTu()));
    connect(txEdit_, SIGNAL(returnPressed()), this, SLOT(SendClicked()));

    RebuildDecoders();
    hide();
}

HvRttyWidget::~HvRttyWidget() { DestroyDecoders(); }

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
    if (!active_ || !samples || count<=0 || !mainDecoder_) return;
    std::string s = mainDecoder_->push(samples,count);
    if (!s.empty()) Consume(mainBuffer_,s,mark_->value(),false);
    if (!multi_->isChecked()) return;
    for (int i=0;i<lanes_.size();++i) {
        Lane *l=lanes_[i];
        std::string m=l->decoder->push(samples,count);
        if (!m.empty()) Consume(l->buffer,m,l->mark,true);
    }
}

void HvRttyWidget::SetActive(bool on)
{
    active_=on;
    if (on) { show(); raise(); activateWindow(); stateLabel_->setText("RX / RTTY ACTIVE"); }
    else { stateLabel_->setText("RX"); hide(); }
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
void HvRttyWidget::MacroCq() { txEdit_->setText("CQ CQ PU2BRU PU2BRU CQ"); }
void HvRttyWidget::MacroExch() { txEdit_->setText("599 001 001"); }
void HvRttyWidget::MacroTu() { txEdit_->setText("TU PU2BRU CQ"); }
