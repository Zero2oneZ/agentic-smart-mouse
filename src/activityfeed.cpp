#include "backend.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>

QVariantList Backend::activityPage(qint64 before) const {
    auto rows=eventPage(m_current,before);
    for(auto &value:rows){auto row=value.toMap();QSqlQuery q(m_db);
        if(row["source"].toString().startsWith("message:")){
            q.prepare("SELECT document FROM message_origins WHERE message=?");q.addBindValue(row["source"].toString().mid(8).toInt());
        }else{q.prepare("SELECT document FROM event_origins WHERE event=?");q.addBindValue(row["id"]);}
        if(q.exec()&&q.next())row["origin"]=QJsonDocument::fromJson(q.value(0).toByteArray()).object().toVariantMap();
        value=row;
    }
    if(before==0&&busy()&&m_ledger->contains(m_current,m_replyCategory))rows.prepend(QVariantMap{{"id",-m_replyMessage},{"category",m_replyCategory},{"name",field(m_replyCategory,"name")},{"kind","chat.streaming"},{"summary",tr("Assistant responding")},{"session","live"},{"source","live"},{"created",""}});
    return rows;
}
QVariantMap Backend::activityDetail(qint64 id,int offset) const {
    if(id>=0)return readEvent(id,offset);
    if(id!=-m_replyMessage||!busy()||!m_ledger->contains(m_current,m_replyCategory))return {{"error",tr("This live response is no longer active.")}};
    return {{"content",SessionLedger::redact(m_answer).mid(qMax(0,offset),6000)},{"nextOffset",-1}};
}
