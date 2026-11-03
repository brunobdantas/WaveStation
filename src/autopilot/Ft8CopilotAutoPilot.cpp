#include "Ft8CopilotAutoPilot.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

#define _BANDS_H_
#define _LAMBDA_H_
#include "../config_band_all.h"
#undef _BANDS_H_
#undef _LAMBDA_H_

namespace {
QString upperTrim(const QString &s) { return s.trimmed().toUpper(); }

QString cleanCtyToken(QString token)
{
    token = token.trimmed().toUpper();
    if (token.startsWith("=")) token.remove(0,1);
    const QString stops = "([{<~";
    int cut = token.size();
    for (QChar ch : stops) {
        const int p = token.indexOf(ch);
        if (p >= 0 && p < cut) cut = p;
    }
    token = token.left(cut);
    while (token.endsWith(";") || token.endsWith(",")) token.chop(1);
    return token;
}
}

Ft8CopilotAutoPilot::Ft8CopilotAutoPilot(const QString &appPath, QWidget *parent)
    : QFrame(parent), appPath_(appPath)
{
    selectorChain_ << "Any";
    loadCty();
    loadHistory();
    loadSettings();
    buildUi();

    decisionTimer_.setSingleShot(true);
    decisionTimer_.setInterval(350);
    connect(&decisionTimer_, SIGNAL(timeout()), this, SLOT(decideNow()));

    watchdogTimer_.setInterval(1000);
    connect(&watchdogTimer_, SIGNAL(timeout()), this, SLOT(watchdogTick()));
    watchdogTimer_.start();
    updateStatus();
}

void Ft8CopilotAutoPilot::buildUi()
{
    QHBoxLayout *l = new QHBoxLayout(this);
    l->setContentsMargins(6,2,6,2);
    l->setSpacing(6);

    QLabel *brand = new QLabel("FT8CoPilot AutoPilot", this);
    QFont bf = brand->font();
    bf.setBold(true);
    brand->setFont(bf);

    runButton_ = new QPushButton("RUN", this);
    settingsButton_ = new QPushButton("Settings", this);
    stateLabel_ = new QLabel("PAUSED", this);
    targetLabel_ = new QLabel("No target", this);

    l->addWidget(brand);
    l->addWidget(runButton_);
    l->addWidget(settingsButton_);
    l->addWidget(stateLabel_);
    l->addStretch();
    l->addWidget(targetLabel_);

    connect(runButton_, SIGNAL(clicked()), this, SLOT(toggleRun()));
    connect(settingsButton_, SIGNAL(clicked()), this, SLOT(openSettings()));
}

void Ft8CopilotAutoPilot::buildSettingsDialog()
{
    if (settingsDialog_) return;

    settingsDialog_ = new QDialog(this);
    settingsDialog_->setWindowTitle("FT8CoPilot AutoPilot 0.1.0");
    QVBoxLayout *root = new QVBoxLayout(settingsDialog_);
    QFormLayout *form = new QFormLayout();

    chainEdit_ = new QLineEdit(selectorChain_.join(","), settingsDialog_);
    chainEdit_->setToolTip("Any, CallSign, Grid, Continent, Country, CQZone, ITUZone, DXCC100, Extra");

    minSnrSpin_ = new QSpinBox(settingsDialog_);
    minSnrSpin_->setRange(-50,49);
    minSnrSpin_->setValue(minSnr_);
    maxSnrSpin_ = new QSpinBox(settingsDialog_);
    maxSnrSpin_->setRange(-49,50);
    maxSnrSpin_->setValue(maxSnr_);

    candidateSpin_ = new QSpinBox(settingsDialog_);
    candidateSpin_->setRange(5,120);
    candidateSpin_->setSuffix(" s");
    candidateSpin_->setValue(candidateSeconds_);

    retrySpin_ = new QSpinBox(settingsDialog_);
    retrySpin_->setRange(1,240);
    retrySpin_->setSuffix(" min");
    retrySpin_->setValue(retryMinutes_);

    txRetriesSpin_ = new QSpinBox(settingsDialog_);
    txRetriesSpin_->setRange(1,20);
    txRetriesSpin_->setValue(txRetries_);

    considerRR73Box_ = new QCheckBox("Consider stations sending RR73/73", settingsDialog_);
    considerRR73Box_->setChecked(considerRR73_);
    followFrequencyBox_ = new QCheckBox("Follow decoded audio frequency", settingsDialog_);
    followFrequencyBox_->setChecked(followFrequency_);

    blacklistEdit_ = new QLineEdit(blacklist_.join(","), settingsDialog_);
    callSpecEdit_ = new QLineEdit(callSpec_, settingsDialog_);
    callSpecEdit_->setPlaceholderText("comma calls or re:^PY|^LU");
    gridRegexEdit_ = new QLineEdit(gridRegex_, settingsDialog_);
    gridRegexEdit_->setPlaceholderText("e.g. ^G[GH]");
    continentEdit_ = new QLineEdit(continentList_.join(","), settingsDialog_);
    continentEdit_->setPlaceholderText("SA,NA,EU");
    countryEdit_ = new QLineEdit(countryList_.join(","), settingsDialog_);
    cqZoneEdit_ = new QLineEdit(settingsDialog_);
    ituZoneEdit_ = new QLineEdit(settingsDialog_);
    dxccWorkedSpin_ = new QSpinBox(settingsDialog_);
    dxccWorkedSpin_->setRange(1,99);
    dxccWorkedSpin_->setValue(dxccWorkedCount_);
    extraEdit_ = new QLineEdit(extraList_.join(","), settingsDialog_);
    extraEdit_->setPlaceholderText("POTA,DX,QRP");

    QStringList cq; for (int z : cqZones_) cq << QString::number(z);
    QStringList itu; for (int z : ituZones_) itu << QString::number(z);
    cqZoneEdit_->setText(cq.join(","));
    ituZoneEdit_->setText(itu.join(","));

    form->addRow("Selector chain:", chainEdit_);
    form->addRow("Min SNR (exclusive):", minSnrSpin_);
    form->addRow("Max SNR (exclusive):", maxSnrSpin_);
    form->addRow("Candidate lifetime:", candidateSpin_);
    form->addRow("Retry cooldown:", retrySpin_);
    form->addRow("TX retry budget:", txRetriesSpin_);
    form->addRow(considerRR73Box_);
    form->addRow(followFrequencyBox_);
    form->addRow("Blacklist:", blacklistEdit_);
    form->addRow("CallSign selector:", callSpecEdit_);
    form->addRow("Grid selector regex:", gridRegexEdit_);
    form->addRow("Continent list:", continentEdit_);
    form->addRow("Country list:", countryEdit_);
    form->addRow("CQ zones:", cqZoneEdit_);
    form->addRow("ITU zones:", ituZoneEdit_);
    form->addRow("DXCC100 worked count:", dxccWorkedSpin_);
    form->addRow("Extra list:", extraEdit_);
    root->addLayout(form);

    QLabel *note = new QLabel(
        "The first selector in the chain that finds an eligible station wins. "
        "AutoPilot always starts PAUSED.", settingsDialog_);
    note->setWordWrap(true);
    root->addWidget(note);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, settingsDialog_);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, settingsDialog_, [this]() {
        selectorChain_ = csv(chainEdit_->text());
        if (selectorChain_.isEmpty()) selectorChain_ << "Any";
        minSnr_ = minSnrSpin_->value();
        maxSnr_ = maxSnrSpin_->value();
        candidateSeconds_ = candidateSpin_->value();
        retryMinutes_ = retrySpin_->value();
        txRetries_ = txRetriesSpin_->value();
        considerRR73_ = considerRR73Box_->isChecked();
        followFrequency_ = followFrequencyBox_->isChecked();
        blacklist_ = csv(blacklistEdit_->text());
        callSpec_ = callSpecEdit_->text().trimmed();
        gridRegex_ = gridRegexEdit_->text().trimmed();
        continentList_ = csv(continentEdit_->text());
        countryList_ = csv(countryEdit_->text());
        cqZones_ = intSet(cqZoneEdit_->text());
        ituZones_ = intSet(ituZoneEdit_->text());
        dxccWorkedCount_ = dxccWorkedSpin_->value();
        extraList_ = csv(extraEdit_->text());
        saveSettings();
        settingsDialog_->accept();
    });
    connect(buttons, SIGNAL(rejected()), settingsDialog_, SLOT(reject()));
}

void Ft8CopilotAutoPilot::loadSettings()
{
    QSettings s(appPath_ + "/settings/ft8copilot_autopilot.ini", QSettings::IniFormat);
    selectorChain_ = csv(s.value("selectors/chain","Any").toString());
    if (selectorChain_.isEmpty()) selectorChain_ << "Any";
    minSnr_ = s.value("base/min_snr",-18).toInt();
    maxSnr_ = s.value("base/max_snr",3).toInt();
    candidateSeconds_ = s.value("base/candidate_seconds",29).toInt();
    retryMinutes_ = s.value("base/retry_minutes",15).toInt();
    txRetries_ = s.value("base/tx_retries",5).toInt();
    considerRR73_ = s.value("base/consider_rr73",false).toBool();
    followFrequency_ = s.value("base/follow_frequency",false).toBool();
    blacklist_ = csv(s.value("base/blacklist","").toString());
    callSpec_ = s.value("selector/callsign","").toString();
    gridRegex_ = s.value("selector/grid","").toString();
    continentList_ = csv(s.value("selector/continents","").toString());
    countryList_ = csv(s.value("selector/countries","").toString());
    cqZones_ = intSet(s.value("selector/cq_zones","").toString());
    ituZones_ = intSet(s.value("selector/itu_zones","").toString());
    dxccWorkedCount_ = s.value("selector/dxcc100_worked_count",4).toInt();
    extraList_ = csv(s.value("selector/extra","").toString());
    running_ = false;
}

void Ft8CopilotAutoPilot::saveSettings()
{
    QSettings s(appPath_ + "/settings/ft8copilot_autopilot.ini", QSettings::IniFormat);
    s.setValue("selectors/chain", selectorChain_.join(","));
    s.setValue("base/min_snr", minSnr_);
    s.setValue("base/max_snr", maxSnr_);
    s.setValue("base/candidate_seconds", candidateSeconds_);
    s.setValue("base/retry_minutes", retryMinutes_);
    s.setValue("base/tx_retries", txRetries_);
    s.setValue("base/consider_rr73", considerRR73_);
    s.setValue("base/follow_frequency", followFrequency_);
    s.setValue("base/blacklist", blacklist_.join(","));
    s.setValue("selector/callsign", callSpec_);
    s.setValue("selector/grid", gridRegex_);
    s.setValue("selector/continents", continentList_.join(","));
    s.setValue("selector/countries", countryList_.join(","));
    QStringList cq; for (int z : cqZones_) cq << QString::number(z);
    QStringList itu; for (int z : ituZones_) itu << QString::number(z);
    s.setValue("selector/cq_zones", cq.join(","));
    s.setValue("selector/itu_zones", itu.join(","));
    s.setValue("selector/dxcc100_worked_count", dxccWorkedCount_);
    s.setValue("selector/extra", extraList_.join(","));
    s.sync();
}

void Ft8CopilotAutoPilot::toggleRun()
{
    running_ = !running_;
    if (!running_) {
        decisionTimer_.stop();
        clearActive(false, true);
    } else {
        scheduleDecision();
    }
    updateStatus();
}

void Ft8CopilotAutoPilot::openSettings()
{
    buildSettingsDialog();
    chainEdit_->setText(selectorChain_.join(","));
    minSnrSpin_->setValue(minSnr_);
    maxSnrSpin_->setValue(maxSnr_);
    candidateSpin_->setValue(candidateSeconds_);
    retrySpin_->setValue(retryMinutes_);
    txRetriesSpin_->setValue(txRetries_);
    considerRR73Box_->setChecked(considerRR73_);
    followFrequencyBox_->setChecked(followFrequency_);
    blacklistEdit_->setText(blacklist_.join(","));
    callSpecEdit_->setText(callSpec_);
    gridRegexEdit_->setText(gridRegex_);
    continentEdit_->setText(continentList_.join(","));
    countryEdit_->setText(countryList_.join(","));
    QStringList cq; for (int z : cqZones_) cq << QString::number(z);
    QStringList itu; for (int z : ituZones_) itu << QString::number(z);
    cqZoneEdit_->setText(cq.join(","));
    ituZoneEdit_->setText(itu.join(","));
    dxccWorkedSpin_->setValue(dxccWorkedCount_);
    extraEdit_->setText(extraList_.join(","));
    settingsDialog_->exec();
}

void Ft8CopilotAutoPilot::setStationIdentity(QString call, QString grid)
{
    myCall_ = baseCall(call);
    myGrid_ = upperTrim(grid);
    myEntity_ = resolveEntity(myCall_);
}

void Ft8CopilotAutoPilot::setBand(QString band)
{
    const QString n = normalizeBand(band);
    if (n == myBand_) return;
    myBand_ = n;
    candidates_.clear();
    clearActive(false, true);
    updateStatus();
}

void Ft8CopilotAutoPilot::setModeId(int modeId)
{
    const QString m = modeName(modeId);
    if (m == myMode_) return;
    myMode_ = m;
    candidates_.clear();
    clearActive(false, true);
    updateStatus();
}

void Ft8CopilotAutoPilot::onDecode(QStringList decode)
{
    emit forwardDecode(decode);
    if (decode.size() < 10) return;

    const QString msg = decode.value(4).simplified().toUpper();
    const QStringList t = msg.split(' ', Qt::SkipEmptyParts);
    if (t.isEmpty()) return;

    if (!activeCall_.isEmpty() && t.size() >= 3 &&
        looksLikeCall(t.value(0)) && looksLikeCall(t.value(1))) {
        const QString to = baseCall(t.value(0));
        const QString from = baseCall(t.value(1));
        if (from == activeCall_ && to != myCall_) clearActive(true, true);
    }

    Candidate c;
    c.rawMessage = msg;
    c.decodeTime = decode.value(0);
    c.audioFreq = decode.value(9);
    c.band = myBand_;
    c.mode = myMode_;
    c.seenUtc = QDateTime::currentDateTimeUtc();
    bool ok=false;
    c.snr = decode.value(1).toInt(&ok);
    if (!ok) c.snr = -50;

    if (t.value(0) == "CQ") {
        int callIndex = -1;
        for (int i=1; i<t.size(); ++i) {
            if (looksLikeCall(t.at(i))) { callIndex=i; break; }
        }
        if (callIndex < 0) return;
        c.call = baseCall(t.at(callIndex));
        if (callIndex > 1) c.extra = t.at(1);
        for (int i=t.size()-1; i>=0; --i) {
            if (looksLikeGrid(t.at(i))) { c.grid=t.at(i); break; }
        }
    } else if (considerRR73_ && t.size() >= 3 &&
               looksLikeCall(t.value(0)) && looksLikeCall(t.value(1)) &&
               (t.value(2) == "RR73" || t.value(2) == "73")) {
        if (baseCall(t.value(0)) == myCall_) return;
        c.call = baseCall(t.value(1));
        c.extra = "RR73";
    } else {
        return;
    }

    if (c.call.isEmpty() || c.call == myCall_) return;
    c.entity = resolveEntity(c.call);

    const Candidate old = candidates_.value(c.call);
    if (c.grid.isEmpty()) c.grid = old.grid;
    if (c.extra.isEmpty()) c.extra = old.extra;
    candidates_.insert(c.call,c);

    pruneCandidates();
    if (running_) scheduleDecision();
    updateStatus();
}

void Ft8CopilotAutoPilot::onQsoProgress(int progress)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (progress > 0) {
        if (!activeCall_.isEmpty()) {
            activeEngaged_ = true;
            activeLastProgressUtc_ = now;
        } else {
            manualBusy_ = true;
            activeLastProgressUtc_ = now;
        }
    } else if (manualBusy_ && activeCall_.isEmpty()) {
        manualBusy_ = false;
    }
}

void Ft8CopilotAutoPilot::onManualTarget(QString, QString selectedCall, QString, QString, QString)
{
    const QString c = baseCall(selectedCall);
    if (c.isEmpty()) return;
    manualBusy_ = true;
    activeCall_ = c;
    activeStartedUtc_ = QDateTime::currentDateTimeUtc();
    activeLastProgressUtc_ = activeStartedUtc_;
    activeEngaged_ = false;
    updateStatus();
}

void Ft8CopilotAutoPilot::onAdifRecord(QString adif)
{
    HistoryRow h;
    h.call = baseCall(adifValue(adif,"CALL"));
    h.band = normalizeBand(adifValue(adif,"BAND"));
    h.country = adifValue(adif,"COUNTRY");
    h.whenUtc = adifTime(adif);
    if (!h.call.isEmpty()) history_.push_back(h);

    activeCall_.clear();
    activeEngaged_ = false;
    manualBusy_ = false;
    if (running_) scheduleDecision();
    updateStatus();
}

void Ft8CopilotAutoPilot::scheduleDecision()
{
    if (!running_ || !activeCall_.isEmpty() || manualBusy_) return;
    if (myMode_ != "FT8" && myMode_ != "FT4" && myMode_ != "FT2") return;
    decisionTimer_.start();
}

void Ft8CopilotAutoPilot::decideNow()
{
    if (!running_ || !activeCall_.isEmpty() || manualBusy_) return;
    pruneCandidates();
    bool ok=false;
    const Candidate c = chooseCandidate(&ok);
    if (!ok) { updateStatus(); return; }

    activeCall_ = c.call;
    activeStartedUtc_ = QDateTime::currentDateTimeUtc();
    activeLastProgressUtc_ = activeStartedUtc_;
    activeEngaged_ = false;

    emit ensureAutoRequested();
    emit selectTarget(c.rawMessage, c.call, c.decodeTime, QString::number(c.snr),
                      followFrequency_ ? c.audioFreq : QString());
    updateStatus();
}

void Ft8CopilotAutoPilot::watchdogTick()
{
    pruneCandidates();
    if (activeCall_.isEmpty()) {
        if (running_) scheduleDecision();
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (!activeStartedUtc_.isValid()) activeStartedUtc_ = now;

    if (manualBusy_ && !running_) {
        if (activeStartedUtc_.secsTo(now) > 180) {
            activeCall_.clear();
            manualBusy_ = false;
            updateStatus();
        }
        return;
    }

    if (!activeEngaged_) {
        if (activeStartedUtc_.msecsTo(now) / 1000.0 > txTimeoutSeconds())
            clearActive(true, true);
    } else if (activeLastProgressUtc_.isValid() &&
               activeLastProgressUtc_.secsTo(now) > 120) {
        clearActive(true, true);
    }
}

void Ft8CopilotAutoPilot::clearActive(bool cooldownTarget, bool halt)
{
    const QString old = activeCall_;
    if (cooldownTarget && !old.isEmpty())
        cooldown_.insert(old, QDateTime::currentDateTimeUtc().addSecs(retryMinutes_ * 60));
    if (halt && !old.isEmpty()) emit haltTxRequested();
    activeCall_.clear();
    activeEngaged_ = false;
    manualBusy_ = false;
    if (running_) scheduleDecision();
    updateStatus();
}

void Ft8CopilotAutoPilot::pruneCandidates()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (auto it=candidates_.begin(); it!=candidates_.end();) {
        if (!it.value().seenUtc.isValid() || it.value().seenUtc.secsTo(now) > candidateSeconds_)
            it = candidates_.erase(it);
        else ++it;
    }
    for (auto it=cooldown_.begin(); it!=cooldown_.end();) {
        if (!it.value().isValid() || it.value() <= now) it = cooldown_.erase(it);
        else ++it;
    }
}

bool Ft8CopilotAutoPilot::workedCallBand(const Candidate &c) const
{
    for (const HistoryRow &h : history_) {
        if (baseCall(h.call) == baseCall(c.call) &&
            normalizeBand(h.band) == normalizeBand(c.band))
            return true;
    }
    return false;
}

bool Ft8CopilotAutoPilot::baseEligible(const Candidate &c) const
{
    if (c.call.isEmpty()) return false;
    if (c.snr <= minSnr_ || c.snr >= maxSnr_) return false;
    if (listContainsCI(blacklist_,c.call)) return false;
    if (workedCallBand(c)) return false;
    if (cooldown_.contains(c.call) &&
        cooldown_.value(c.call) > QDateTime::currentDateTimeUtc()) return false;
    if (!c.seenUtc.isValid() ||
        c.seenUtc.secsTo(QDateTime::currentDateTimeUtc()) > candidateSeconds_) return false;

    if (c.extra.compare("DX",Qt::CaseInsensitive)==0 &&
        myEntity_.valid && c.entity.valid &&
        myEntity_.continent == c.entity.continent)
        return false;

    return true;
}

bool Ft8CopilotAutoPilot::selectorMatches(const QString &selector, const Candidate &c) const
{
    const QString s = selector.trimmed().toUpper();
    if (s == "ANY") return true;

    if (s == "CALLSIGN") {
        if (callSpec_.trimmed().isEmpty()) return false;
        if (callSpec_.trimmed().startsWith("re:",Qt::CaseInsensitive)) {
            QRegularExpression re(callSpec_.trimmed().mid(3),QRegularExpression::CaseInsensitiveOption);
            return re.isValid() && re.match(c.call).hasMatch();
        }
        return listContainsCI(csv(callSpec_),c.call);
    }

    if (s == "GRID") {
        if (gridRegex_.isEmpty() || c.grid.isEmpty()) return false;
        QRegularExpression re(gridRegex_,QRegularExpression::CaseInsensitiveOption);
        return re.isValid() && re.match(c.grid).hasMatch();
    }

    if (s == "CONTINENT")
        return c.entity.valid && listContainsCI(continentList_,c.entity.continent);
    if (s == "COUNTRY")
        return c.entity.valid && listContainsCI(countryList_,c.entity.country);
    if (s == "CQZONE")
        return c.entity.valid && cqZones_.contains(c.entity.cqZone);
    if (s == "ITUZONE")
        return c.entity.valid && ituZones_.contains(c.entity.ituZone);
    if (s == "DXCC100")
        return c.entity.valid && workedCountryCount(c.entity.country,c.band) < dxccWorkedCount_;
    if (s == "EXTRA")
        return listContainsCI(extraList_,c.extra);
    return false;
}

Ft8CopilotAutoPilot::Candidate Ft8CopilotAutoPilot::chooseCandidate(bool *ok) const
{
    *ok = false;
    QVector<Candidate> base;
    for (auto it=candidates_.constBegin(); it!=candidates_.constEnd(); ++it)
        if (baseEligible(it.value())) base.push_back(it.value());

    std::stable_sort(base.begin(),base.end(),[](const Candidate &a,const Candidate &b){
        if (a.snr != b.snr) return a.snr > b.snr;
        return a.call < b.call;
    });

    for (const QString &selector : selectorChain_) {
        for (const Candidate &c : base) {
            if (selectorMatches(selector,c)) {
                *ok = true;
                return c;
            }
        }
    }
    return Candidate();
}

int Ft8CopilotAutoPilot::workedCountryCount(const QString &country, const QString &band) const
{
    if (country.isEmpty()) return 0;
    int count=0;
    for (const HistoryRow &h : history_) {
        if (normalizeBand(h.band) != normalizeBand(band)) continue;
        QString hc=h.country;
        if (hc.isEmpty()) hc=resolveEntity(h.call).country;
        if (hc.compare(country,Qt::CaseInsensitive)==0) ++count;
    }
    return count;
}

void Ft8CopilotAutoPilot::loadHistory()
{
    history_.clear();
    QFile f(appPath_ + "/log/mshvlog.adi");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    const QString all = QString::fromUtf8(f.readAll());
    const QStringList records = all.split(
        QRegularExpression("<\\s*EOR\\s*>",QRegularExpression::CaseInsensitiveOption),
        Qt::SkipEmptyParts);
    for (const QString &adif : records) {
        HistoryRow h;
        h.call = baseCall(adifValue(adif,"CALL"));
        if (h.call.isEmpty()) continue;
        h.band = normalizeBand(adifValue(adif,"BAND"));
        h.country = adifValue(adif,"COUNTRY");
        h.whenUtc = adifTime(adif);
        history_.push_back(h);
    }
}

void Ft8CopilotAutoPilot::loadCty()
{
    cty_.clear();
    QFile f(QCoreApplication::applicationDirPath() + "/settings/database/cty.dat");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QString logical;
    while (!f.atEnd()) {
        QString line=QString::fromUtf8(f.readLine());
        line.remove('\r'); line.remove('\n');
        logical += line;
        if (!logical.endsWith(';')) continue;
        logical.chop(1);
        const QStringList fields=logical.split(':');
        logical.clear();
        if (fields.size()<8) continue;

        Entity base;
        base.country=fields.value(0).trimmed();
        base.cqZone=fields.value(1).trimmed().toInt();
        base.ituZone=fields.value(2).trimmed().toInt();
        base.continent=fields.value(3).trimmed().toUpper();
        base.valid=true;

        const QString primary=cleanCtyToken(fields.value(7));
        if (!primary.isEmpty()) {
            Entity e=base; e.prefix=primary;
            CtyRecord r; r.key=primary; r.entity=e; r.exact=false;
            cty_.push_back(r);
        }
        if (fields.size()>8) {
            const QStringList aliases=fields.value(8).split(',',Qt::SkipEmptyParts);
            for (QString token : aliases) {
                const bool exact=token.trimmed().startsWith('=');
                const QString key=cleanCtyToken(token);
                if (key.isEmpty()) continue;
                Entity e=base; e.prefix=key;
                CtyRecord r; r.key=key; r.entity=e; r.exact=exact;
                cty_.push_back(r);
            }
        }
    }
}

Ft8CopilotAutoPilot::Entity Ft8CopilotAutoPilot::resolveEntity(const QString &call) const
{
    const QString c=baseCall(call);
    for (const CtyRecord &r : cty_)
        if (r.exact && c==r.key) return r.entity;

    int best=-1;
    Entity out;
    for (const CtyRecord &r : cty_) {
        if (r.exact) continue;
        if (c.startsWith(r.key) && r.key.size()>best) {
            best=r.key.size();
            out=r.entity;
        }
    }
    return out;
}

QString Ft8CopilotAutoPilot::baseCall(const QString &call)
{
    const QString c=upperTrim(call);
    const QStringList parts=c.split('/',Qt::SkipEmptyParts);
    QString best;
    for (const QString &p : parts) {
        const QString t=p.trimmed();
        if (!looksLikeCall(t)) continue;
        if (t.size()>best.size()) best=t;
    }
    return best.isEmpty()?c:best;
}

bool Ft8CopilotAutoPilot::looksLikeCall(const QString &token)
{
    static const QRegularExpression re("^(?=.{3,15}$)(?=.*[A-Z])(?=.*\\d)[A-Z0-9/]+$");
    return re.match(token.toUpper()).hasMatch();
}

bool Ft8CopilotAutoPilot::looksLikeGrid(const QString &token)
{
    static const QRegularExpression re("^[A-R]{2}[0-9]{2}([A-X]{2})?$",
                                       QRegularExpression::CaseInsensitiveOption);
    return re.match(token).hasMatch();
}

QString Ft8CopilotAutoPilot::adifValue(const QString &adif, const QString &field)
{
    QRegularExpression re("<"+QRegularExpression::escape(field)+":([0-9]+)(?::[^>]*)?>([^<]*)",
                          QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m=re.match(adif);
    if (!m.hasMatch()) return QString();
    bool ok=false;
    const int len=m.captured(1).toInt(&ok);
    return ok?m.captured(2).left(len).trimmed():QString();
}

QDateTime Ft8CopilotAutoPilot::adifTime(const QString &adif)
{
    const QString d=adifValue(adif,"QSO_DATE");
    QString t=adifValue(adif,"TIME_ON");
    if (d.size()!=8) return QDateTime();
    while (t.size()<6) t+="0";
    QDateTime dt=QDateTime::fromString(d+t.left(6),"yyyyMMddHHmmss");
    dt.setTimeSpec(Qt::UTC);
    return dt;
}

QString Ft8CopilotAutoPilot::normalizeBand(const QString &band)
{
    const QString b=band.trimmed().toUpper();
    for (int i=0;i<COUNT_BANDS;++i) {
        if (lst_bands[i].toUpper()==b || lst_lambda[i].toUpper()==b)
            return lst_lambda[i].toUpper();
    }
    return b;
}

QString Ft8CopilotAutoPilot::modeName(int modeId)
{
    if (modeId==11) return "FT8";
    if (modeId==13) return "FT4";
    if (modeId==18) return "FT2";
    return QString("MODE%1").arg(modeId);
}

QStringList Ft8CopilotAutoPilot::csv(const QString &s)
{
    QStringList out;
    for (QString x : s.split(',',Qt::SkipEmptyParts)) {
        x=x.trimmed();
        if (!x.isEmpty()) out << x;
    }
    return out;
}

QSet<int> Ft8CopilotAutoPilot::intSet(const QString &s)
{
    QSet<int> out;
    for (const QString &x : csv(s)) {
        bool ok=false;
        const int v=x.toInt(&ok);
        if (ok) out.insert(v);
    }
    return out;
}

bool Ft8CopilotAutoPilot::listContainsCI(const QStringList &list, const QString &value)
{
    for (const QString &x : list)
        if (x.trimmed().compare(value.trimmed(),Qt::CaseInsensitive)==0) return true;
    return false;
}

double Ft8CopilotAutoPilot::txTimeoutSeconds() const
{
    double p=15.0;
    if (myMode_=="FT4") p=7.5;
    else if (myMode_=="FT2") p=3.75;
    return qMax(10.0, p * qMax(1,txRetries_) + 5.0);
}

void Ft8CopilotAutoPilot::updateStatus()
{
    if (!runButton_) return;
    runButton_->setText(running_?"PAUSE":"RUN");
    stateLabel_->setText(running_?"RUNNING":"PAUSED");
    QString target=activeCall_.isEmpty()?"No target":("Working "+activeCall_);
    target += QString(" | %1 | %2").arg(myBand_.isEmpty()?"?":myBand_,
                                         myMode_.isEmpty()?"?":myMode_);
    targetLabel_->setText(target);
}
