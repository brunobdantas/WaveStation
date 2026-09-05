// MSHV RTTY integration - Copyright 2026
// GPL-compatible derived integration for MSHV.
#ifndef MSHV_HVRTTYWIDGET_H
#define MSHV_HVRTTYWIDGET_H

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QVector>
#include <QString>
#include "rtty_core.h"

class HvRttyWidget : public QDialog
{
    Q_OBJECT
public:
    explicit HvRttyWidget(QWidget *parent = 0);
    ~HvRttyWidget();

signals:
    void SendRequested(QString text);
    void AbortRequested();
    void TuneRequested(double hz);

public slots:
    void FeedAudio(int *samples, int count);
    void SetActive(bool on);
    void SetMarkFrequency(double hz);
    void SetTxFinished();

private slots:
    void RebuildDecoders();
    void SendClicked();
    void AbortClicked();
    void ClearClicked();
    void MacroCq();
    void MacroExch();
    void MacroTu();

private:
    struct Lane {
        double mark;
        mshv_rtty::Decoder *decoder;
        QString buffer;
        Lane() : mark(0), decoder(0) {}
    };

    QTextEdit *rxText_;
    QLineEdit *txEdit_;
    QDoubleSpinBox *baud_;
    QSpinBox *mark_;
    QSpinBox *shift_;
    QCheckBox *reverse_;
    QCheckBox *multi_;
    QLabel *spaceLabel_;
    QLabel *stateLabel_;
    QPushButton *sendBtn_;
    QPushButton *abortBtn_;
    bool active_;
    mshv_rtty::Decoder *mainDecoder_;
    QString mainBuffer_;
    QVector<Lane*> lanes_;

    mshv_rtty::Config CurrentConfig(double markHz) const;
    void DestroyDecoders();
    void Consume(QString &buffer, const std::string &text, double markHz, bool multi);
    void Flush(QString &buffer, double markHz, bool multi);
    QString Stamp(double markHz, bool multi) const;
};

#endif
