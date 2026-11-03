#pragma once

#include <QDateTime>
#include <QFrame>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVector>

class QCheckBox;
class QDialog;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

class Ft8CopilotAutoPilot final : public QFrame
{
    Q_OBJECT
public:
    explicit Ft8CopilotAutoPilot(const QString &appPath, QWidget *parent = nullptr);

signals:
    void forwardDecode(QStringList decode);
    void selectTarget(QString allText, QString call, QString time, QString txReport, QString freq);
    void ensureAutoRequested();
    void haltTxRequested();

public slots:
    void setStationIdentity(QString call, QString grid);
    void setBand(QString band);
    void setModeId(int modeId);
    void onDecode(QStringList decode);
    void onQsoProgress(int progress);
    void onAdifRecord(QString adif);
    void onManualTarget(QString allText, QString selectedCall, QString time, QString txReport, QString freq);

private slots:
    void toggleRun();
    void openSettings();
    void decideNow();
    void watchdogTick();

private:
    struct Entity {
        QString country;
        QString continent;
        QString prefix;
        int cqZone = 0;
        int ituZone = 0;
        bool valid = false;
    };
    struct CtyRecord {
        QString key;
        Entity entity;
        bool exact = false;
    };
    struct Candidate {
        QString call;
        QString grid;
        QString extra;
        QString rawMessage;
        QString decodeTime;
        QString audioFreq;
        QString band;
        QString mode;
        int snr = -50;
        QDateTime seenUtc;
        Entity entity;
    };
    struct HistoryRow {
        QString call;
        QString band;
        QString country;
        QDateTime whenUtc;
    };

    QString appPath_;
    QString myCall_;
    QString myGrid_;
    QString myBand_;
    QString myMode_;
    Entity myEntity_;

    bool running_ = false;
    bool considerRR73_ = false;
    bool followFrequency_ = false;
    int minSnr_ = -18;
    int maxSnr_ = 3;
    int candidateSeconds_ = 29;
    int retryMinutes_ = 15;
    int txRetries_ = 5;
    int dxccWorkedCount_ = 4;

    QStringList selectorChain_;
    QStringList blacklist_;
    QString callSpec_;
    QString gridRegex_;
    QStringList continentList_;
    QStringList countryList_;
    QSet<int> cqZones_;
    QSet<int> ituZones_;
    QStringList extraList_;

    QHash<QString,Candidate> candidates_;
    QHash<QString,QDateTime> cooldown_;
    QVector<HistoryRow> history_;
    QVector<CtyRecord> cty_;

    QString activeCall_;
    QDateTime activeStartedUtc_;
    QDateTime activeLastProgressUtc_;
    bool activeEngaged_ = false;
    bool manualBusy_ = false;

    QTimer decisionTimer_;
    QTimer watchdogTimer_;

    QPushButton *runButton_ = nullptr;
    QLabel *stateLabel_ = nullptr;
    QLabel *targetLabel_ = nullptr;
    QPushButton *settingsButton_ = nullptr;

    QDialog *settingsDialog_ = nullptr;
    QLineEdit *chainEdit_ = nullptr;
    QSpinBox *minSnrSpin_ = nullptr;
    QSpinBox *maxSnrSpin_ = nullptr;
    QSpinBox *candidateSpin_ = nullptr;
    QSpinBox *retrySpin_ = nullptr;
    QSpinBox *txRetriesSpin_ = nullptr;
    QCheckBox *considerRR73Box_ = nullptr;
    QCheckBox *followFrequencyBox_ = nullptr;
    QLineEdit *blacklistEdit_ = nullptr;
    QLineEdit *callSpecEdit_ = nullptr;
    QLineEdit *gridRegexEdit_ = nullptr;
    QLineEdit *continentEdit_ = nullptr;
    QLineEdit *countryEdit_ = nullptr;
    QLineEdit *cqZoneEdit_ = nullptr;
    QLineEdit *ituZoneEdit_ = nullptr;
    QSpinBox *dxccWorkedSpin_ = nullptr;
    QLineEdit *extraEdit_ = nullptr;

    void buildUi();
    void buildSettingsDialog();
    void loadSettings();
    void saveSettings();
    void loadHistory();
    void loadCty();
    void updateStatus();
    void scheduleDecision();
    void clearActive(bool cooldownTarget, bool halt);
    void pruneCandidates();

    bool baseEligible(const Candidate &c) const;
    bool workedCallBand(const Candidate &c) const;
    bool selectorMatches(const QString &selector, const Candidate &c) const;
    Candidate chooseCandidate(bool *ok) const;
    int workedCountryCount(const QString &country, const QString &band) const;

    Entity resolveEntity(const QString &call) const;
    static QString baseCall(const QString &call);
    static bool looksLikeCall(const QString &token);
    static bool looksLikeGrid(const QString &token);
    static QString adifValue(const QString &adif, const QString &field);
    static QDateTime adifTime(const QString &adif);
    static QString normalizeBand(const QString &band);
    static QString modeName(int modeId);
    static QStringList csv(const QString &s);
    static QSet<int> intSet(const QString &s);
    static bool listContainsCI(const QStringList &list, const QString &value);
    double txTimeoutSeconds() const;
};
