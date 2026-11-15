#include "backend.h"
#include "gitworkspace.h"
#include "sessionledger.h"
#include "theme.h"
#include "translator.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QSqlError>
#include <QSqlQuery>
#include <functional>
#include <QtTest>

class HotboxTests : public QObject {
    Q_OBJECT
  private slots:
    void rollingLedgerMigratesVersionOneAndProtectsContracts() {
        QTemporaryDir dir;
        QString road;
        {
            Backend b(dir.path(), nullptr, false);
            road = b.buildAction("create", {{"schema_version", 1},
                                            {"request_id", "before-migration"},
                                            {"title", "Preserve existing work"}})["roadmap"]
                       .toString();
        }
        auto connection = QUuid::createUuid().toString();
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
            db.setDatabaseName(dir.filePath("hotbox.sqlite3"));
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec("DROP TABLE build_contracts"));
            QVERIFY(q.exec("DELETE FROM build_schema WHERE version=2"));
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);
        {
            Backend b(dir.path(), nullptr, false);
            auto saved = b.buildAction("read", {{"schema_version", 1}, {"roadmap", road}});
            QCOMPARE(saved["title"].toString(), QString("Preserve existing work"));
            auto step = b.buildAction("node.add", {{"schema_version", 1},
                                                   {"request_id", "after-migration"},
                                                   {"roadmap", road},
                                                   {"expected_revision", 1},
                                                   {"kind", "step"},
                                                   {"title", "Contract"}});
            QVERIFY(step["ok"].toBool());
            QVERIFY(b.buildAction("contract.save", {{"schema_version", 1},
                                                    {"request_id", "contract-after-migration"},
                                                    {"roadmap", road},
                                                    {"expected_revision", 2},
                                                    {"node", step["node"]},
                                                    {"acceptance", "Works"},
                                                    {"implementation", "src/buildledger.cpp"},
                                                    {"verification", "Temporary migration test"}})["ok"]
                        .toBool());
        }
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
            db.setDatabaseName(dir.filePath("hotbox.sqlite3"));
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(!q.exec("UPDATE build_contracts SET acceptance='tampered'"));
            QVERIFY(!q.exec("DELETE FROM build_contracts"));
            QVERIFY(q.exec("SELECT MAX(version) FROM build_schema"));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 2);
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);
    }
    void rollingLedgerContractsGuideAndForkEvidence() {
        QTemporaryDir dir;
        Backend b(dir.path(), nullptr, false);
        auto road = b.buildAction(
            "create",
            {{"schema_version", 1}, {"request_id", "rolling"}, {"title", "Rolling work"}})["roadmap"];
        auto read = [&] { return b.buildAction("read", {{"schema_version", 1}, {"roadmap", road}}); };
        int serial = 0;
        auto change = [&](const QString &op, QVariantMap args) {
            args["schema_version"] = 1;
            args["request_id"] = QString("rolling-%1").arg(++serial);
            args["roadmap"] = road;
            args["expected_revision"] = read()["revision"];
            return b.buildAction(op, args);
        };
        auto step = change("node.add", {{"kind", "step"}, {"title", "Implement contract"}})["node"];
        auto guide = [&] {
            return b.buildAction("guide", {{"schema_version", 1}, {"roadmap", road}, {"node", step}});
        };
        QCOMPARE(guide()["items"].toList().first().toMap()["guidance"].toString(),
                 QString("define_contract"));
        auto fields = QVariantMap{{"node", step},
                                  {"acceptance", "Output survives restart"},
                                  {"implementation", "src/buildledger.cpp: BuildLedger::call"},
                                  {"verification", "Run temporary database persistence tests"}};
        auto first = change("contract.save", fields);
        QVERIFY(first["ok"].toBool());
        QCOMPARE(guide()["items"].toList().first().toMap()["guidance"].toString(), QString("implement"));
        QVERIFY(!change("contract.save", fields)["ok"].toBool());
        auto snapshot = change("checkpoint", {});
        QVERIFY(snapshot["ok"].toBool());
        fields["parent_contract"] = first["contract"];
        fields["acceptance"] = "Updated acceptance";
        auto second = change("contract.save", fields);
        QVERIFY(second["ok"].toBool());
        QCOMPARE(read()["contracts"].toList().size(), 2);
        auto references=b.contextTool("read_build_roadmap",{{"roadmap",road}})["contracts"].toList();QVERIFY(!references.first().toMap().contains("acceptance"));
        auto full=b.contextTool("read_build_contract",{{"roadmap",road},{"contract",second["contract"]}});QCOMPARE(full["contract"].toMap()["acceptance"].toString(),QString("Updated acceptance"));
        QCOMPARE(guide()["items"].toList().first().toMap()["contract"].toMap()["id"], second["contract"]);
        auto event = b.recordEvent(
            1, "command.finished", "Check passed",
            QString::fromUtf8(
                QJsonDocument(QJsonObject{{"exitCode", 0}, {"output", QString(9000, 'x')}}).toJson()));
        QVERIFY(change("link",
                       {{"node", step}, {"kind", "terminal_command"}, {"ref", QString::number(event)}})["ok"]
                    .toBool());
        QVERIFY(change("node.update", {{"node", step}, {"state", "recorded"}})["ok"].toBool());
        auto current = guide();
        auto item = current["items"].toList().first().toMap();
        QCOMPARE(item["guidance"].toString(), QString("review_evidence"));
        QCOMPARE(item["verification"].toString(), QString("unverified"));
        QCOMPARE(item["checks"].toList().first().toMap()["outcome"].toString(), QString("command_passed"));
        QCOMPARE(guide()["digest"], current["digest"]);
        auto fork = change("fork", {{"title", "Branch recorded work"}, {"node", step}});
        QVERIFY(fork["ok"].toBool());
        auto branch = b.buildAction("guide", {{"schema_version", 1}, {"roadmap", fork["roadmap"]}});
        auto inherited = branch["items"].toList().first().toMap();
        QVERIFY(inherited["checks"].toList().first().toMap()["inherited"].toBool());
        QCOMPARE(inherited["verification"].toString(), QString("unverified"));
        auto old =
            change("fork", {{"title", "Before contract change"}, {"checkpoint", snapshot["checkpoint"]}});
        auto historical = b.buildAction("read", {{"schema_version", 1}, {"roadmap", old["roadmap"]}});
        QCOMPARE(historical["contracts"].toList().first().toMap()["parent_contract"], first["contract"]);
        QCOMPARE(historical["contracts"].toList().first().toMap()["acceptance"].toString(),
                 QString("Output survives restart"));
        auto idea = change("node.add", {{"kind", "idea"}, {"title", "Future possibility"}});
        QVERIFY(idea["ok"].toBool());
        auto proposal =
            b.buildAction("guide", {{"schema_version", 1}, {"roadmap", road}, {"node", idea["node"]}});
        QCOMPARE(proposal["items"].toList().first().toMap()["guidance"].toString(),
                 QString("review_proposal"));
        for (int i = 0; i < 16; ++i)
            QVERIFY(change("node.add", {{"kind", "addition"}, {"title", "Additional work"}})["ok"].toBool());
        auto bounded = b.buildAction("guide", {{"schema_version", 1}, {"roadmap", road}});
        QVERIFY(bounded["items"].toList().size() <= 12);
        QVERIFY(bounded["omitted"].toInt() > 0);
        QVERIFY(QJsonDocument::fromVariant(bounded).toJson(QJsonDocument::Compact).size() < 20000);
        QVERIFY(b.optimizerReport()["rollingLedger"].toMap()["visibleCount"].toInt() >= 3);
        auto other = b.addCategory("Other", "", "", 0);
        b.select(other);
        QVERIFY(!guide()["ok"].toBool());
        QVERIFY(!b.contextTool("read_build_guidance",{{"roadmap",road}})["ok"].toBool());
    }
    void buildLedgerProtectsImmutableEvidenceAndObjects() {
        QTemporaryDir dir;Backend b(dir.path(),nullptr,false);
        auto road=b.buildAction("create",{{"schema_version",1},{"request_id","make"},{"title","Private docs"}});QVERIFY(road["ok"].toBool());
        auto doc=b.buildAction("document.save",{{"schema_version",1},{"request_id","doc"},{"roadmap",road["roadmap"]},{"expected_revision",1},{"title","Readme"},{"content","immutable content"}});QVERIFY(doc["ok"].toBool());
        auto connection=QUuid::createUuid().toString();
        {
            auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(dir.filePath("hotbox.sqlite3"));QVERIFY(db.open());QSqlQuery q(db);
            QVERIFY(!q.exec("UPDATE build_doc_versions SET content='changed'"));QVERIFY(q.lastError().text().contains("immutable"));QVERIFY(!q.exec("DELETE FROM build_changes"));db.close();
        }
        QSqlDatabase::removeDatabase(connection);
        QFile object(dir.filePath("build-ledger/objects/")+doc["digest"].toString()+".txt");QVERIFY(object.open(QIODevice::WriteOnly|QIODevice::Truncate));object.write("corrupt");object.close();
        auto rejected=b.buildAction("document.save",{{"schema_version",1},{"request_id","repeat-object"},{"roadmap",road["roadmap"]},{"expected_revision",2},{"title","Another"},{"content","immutable content"}});QVERIFY(!rejected["ok"].toBool());
        QCOMPARE(b.buildAction("read",{{"schema_version",1},{"roadmap",road["roadmap"]}})["revision"].toInt(),2);
        QVERIFY(!b.buildAction("node.add",{{"schema_version",1},{"request_id","bad-type"},{"roadmap",road["roadmap"]},{"expected_revision",QString("2")},{"kind","step"},{"title","No coercion"}})["ok"].toBool());
    }
    void agentRunsBindToBuildStepsBeforeLaunch() {
        QTemporaryDir dir;QDir().mkpath(dir.filePath("bin"));QFile fake(dir.filePath("bin/codex"));QVERIFY(fake.open(QIODevice::WriteOnly));fake.write("#!/bin/sh\ncat > received.txt\nprintf 'bound output\\n'\n");fake.close();QVERIFY(fake.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        auto old=qgetenv("PATH");auto restore=qScopeGuard([&]{qputenv("PATH",old);});qputenv("PATH",(dir.filePath("bin")+":/usr/bin").toUtf8());
        Backend b(dir.filePath("data"),nullptr,false);auto road=b.buildAction("create",{{"schema_version",1},{"request_id","road"},{"title","Build"}});QVERIFY(road["ok"].toBool());
        auto step=b.buildAction("node.add",{{"schema_version",1},{"request_id","step"},{"roadmap",road["roadmap"]},{"expected_revision",1},{"kind","step"},{"title","Test"}});QVERIFY(step["ok"].toBool());
        auto target=QVariantMap{{"name","Build"},{"id",1},{"build",road["roadmap"]},{"step",step["node"]}};
        auto id=b.agents()->start("Bound test",target,"codex");QVERIFY(!id.isEmpty());QTRY_COMPARE_WITH_TIMEOUT(b.agents()->selected()["state"].toString(),QString("finished"),5000);
        auto saved=b.buildAction("read",{{"schema_version",1},{"roadmap",road["roadmap"]}});QCOMPARE(saved["links"].toList().size(),1);QCOMPARE(saved["links"].toList().first().toMap()["ref"].toString(),id);
        auto count=b.agents()->jobs().size();target["step"]="missing";QVERIFY(b.agents()->start("Must not launch",target,"codex").isEmpty());QCOMPARE(b.agents()->jobs().size(),count);
        auto context=b.agents()->selected()["target"].toMap()["buildContext"].toMap();QCOMPARE(context["revision"].toInt(),2);QVERIFY(!context["digest"].toString().isEmpty());
        QFile received(dir.filePath("data/agent-runs/")+id+"/received.txt");QVERIFY(received.open(QIODevice::ReadOnly));QVERIFY(received.readAll().contains("buildContext"));
        b.agents()->setBuildContext([&](const QString &r,const QString &n,int){auto snapshot=b.buildAction("guide",{{"schema_version",1},{"roadmap",r},{"node",n}});b.buildAction("node.add",{{"schema_version",1},{"request_id","race"},{"roadmap",r},{"expected_revision",snapshot["revision"]},{"kind","issue"},{"title","Concurrent change"}});return snapshot;});
        target["step"]=step["node"];QVERIFY(b.agents()->start("Stale guidance must not launch",target,"codex").isEmpty());auto failed=b.agents()->jobs().first().toMap();QVERIFY(!QFileInfo(dir.filePath("data/agent-runs/")+failed["id"].toString()+"/received.txt").exists());
    }
    void buildLedgerVersionsForksAndScopes() {
        QTemporaryDir dir;QString road,node,version,checkpoint;
        {
            Backend b(dir.path(),nullptr,false);auto workspace=b.addCategory("Ledger project","","",0);
            auto create=QVariantMap{{"schema_version",1},{"request_id","create-one"},{"title","Build plan"},{"branch_ref","main"}};
            auto first=b.buildAction("create",create);QVERIFY2(first["ok"].toBool(),qPrintable(first["error"].toString()));road=first["roadmap"].toString();
            QCOMPARE(b.buildAction("create",create)["roadmap"].toString(),road);QVERIFY(b.buildAction("create",create)["replayed"].toBool());
            create["title"]="Different";QVERIFY(!b.buildAction("create",create)["ok"].toBool());
            auto read=[&]{return b.buildAction("read",{{"schema_version",1},{"roadmap",road}});};
            int serial=0;auto change=[&](const QString &op,QVariantMap args){args["schema_version"]=1;args["request_id"]=QString("change-%1").arg(++serial);args["roadmap"]=road;args["expected_revision"]=read()["revision"];return b.buildAction(op,args);};
            auto milestone=change("node.add",{{"kind","milestone"},{"title","Working build"}});QVERIFY(milestone["ok"].toBool());
            auto step=change("node.add",{{"kind","step"},{"parent",milestone["node"]},{"title","Build API"}});QVERIFY(step["ok"].toBool());node=step["node"].toString();
            QVERIFY(change("node.add",{{"kind","substep"},{"parent",node},{"title","Validate requests"}})["ok"].toBool());
            auto revision=read()["revision"];QVERIFY(!change("node.update",{{"node",node},{"state","verified"}})["ok"].toBool());QCOMPARE(read()["revision"],revision);
            QVERIFY(!b.buildAction("node.add",{{"schema_version",1},{"request_id","stale"},{"roadmap",road},{"expected_revision",1},{"kind","step"},{"title","Wrong revision"}})["ok"].toBool());
            auto event=b.recordEvent(workspace,"command.finished","Tests failed","exit 1");QVERIFY(change("link",{{"node",node},{"kind","terminal_command"},{"ref",QString::number(event)}})["ok"].toBool());
            auto doc=change("document.save",{{"node",node},{"title","API contract"},{"content",QString("password=private-example\n")+QString(7000,'a')}});QVERIFY2(doc["ok"].toBool(),qPrintable(doc["error"].toString()));version=doc["version"].toString();
            auto content=b.buildAction("document.read",{{"schema_version",1},{"roadmap",road},{"version",version}});QVERIFY(!content["content"].toString().contains("private-example"));QVERIFY(content["nextOffset"].toInt()>0);
            auto saved=change("checkpoint",{});QVERIFY(saved["ok"].toBool());checkpoint=saved["checkpoint"].toString();
            QVERIFY(change("document.save",{{"document",doc["document"]},{"parent_version",version},{"content","second version"}})["ok"].toBool());
            QVERIFY(!change("document.save",{{"document",doc["document"]},{"parent_version",version},{"content","stale version"}})["ok"].toBool());
            auto sourceRevision=read()["revision"];auto fork=change("fork",{{"title","Alternative"},{"branch_ref","experiment"},{"checkpoint",checkpoint}});QVERIFY2(fork["ok"].toBool(),qPrintable(fork["error"].toString()));QCOMPARE(read()["revision"],sourceRevision);
            auto branch=b.buildAction("read",{{"schema_version",1},{"roadmap",fork["roadmap"]}});QCOMPARE(branch["nodes"].toList().size(),3);QCOMPARE(branch["links"].toList().size(),1);
            auto forkVersion=branch["documents"].toList().first().toMap();QCOMPARE(forkVersion["parent_version"].toString(),version);QCOMPARE(forkVersion["number"].toInt(),1);
            auto forkContent=b.buildAction("document.read",{{"schema_version",1},{"roadmap",fork["roadmap"]},{"version",forkVersion["version"]}});QVERIFY(forkContent["content"].toString()!=QString("second version"));
            auto piece=change("fork",{{"title","API branch"},{"node",node},{"checkpoint",checkpoint}});QVERIFY(piece["ok"].toBool());auto subtree=b.buildAction("read",{{"schema_version",1},{"roadmap",piece["roadmap"]}});QCOMPARE(subtree["nodes"].toList().size(),2);QCOMPARE(subtree["base_node"].toString(),node);QVERIFY(subtree["nodes"].toList().first().toMap()["parent"].toString().isEmpty());
            auto other=b.addCategory("Outside","","",0);b.select(other);QVERIFY(!read()["ok"].toBool());QVERIFY(b.contextTool("read_build_roadmap",{{"roadmap",road}}).contains("error"));
            b.select(1);QVERIFY(read()["ok"].toBool());QVERIFY(b.contextTool("read_build_document",{{"roadmap",road},{"version",version},{"offset",0}})["ok"].toBool());
            auto history=b.buildAction("history",{{"schema_version",1},{"roadmap",road}});QVERIFY(!history["changes"].toList().isEmpty());QCOMPARE(first["master_chat"].toInt(),1);
        }
        {
            Backend b(dir.path(),nullptr,false);auto result=b.buildAction("read",{{"schema_version",1},{"roadmap",road}});QVERIFY(result["ok"].toBool());QCOMPARE(result["documents"].toList().size(),2);
            QVERIFY(b.buildAction("checkpoint.read",{{"schema_version",1},{"roadmap",road},{"checkpoint",checkpoint}})["ok"].toBool());
            Theme theme(dir.path());Translator translator(dir.filePath("language"),dir.path());QQmlApplicationEngine engine;
            engine.rootContext()->setContextProperty("backend",&b);engine.rootContext()->setContextProperty("theme",&theme);engine.rootContext()->setContextProperty("translator",&translator);engine.load(QUrl("qrc:/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
            auto *window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());window->setProperty("page",QString("builds"));auto *view=window->findChild<QObject*>("buildLedgerView");QVERIFY(view);QVERIFY(QMetaObject::invokeMethod(view,"choose",Q_ARG(QVariant,QVariant(road))));window->show();QTest::qWait(100);QVERIFY(window->grabWindow().save("/tmp/hotbox-build-ledger-preview.png"));
        }
    }
    void masterLoopTracksReportedUsageAndParentBudgets() {
        QTemporaryDir dir;QDir().mkpath(dir.filePath("bin"));QFile fake(dir.filePath("bin/codex"));
        auto write=[&](int input){if(!fake.open(QIODevice::WriteOnly|QIODevice::Truncate))return false;fake.write(("#!/bin/sh\ncat >/dev/null\nprintf '%s\\n' '{\"type\":\"turn.completed\",\"usage\":{\"input_tokens\":"+QString::number(input)+",\"output_tokens\":3}}'\n").toUtf8());fake.close();return fake.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);};QVERIFY(write(12));
        const auto old=qgetenv("PATH");auto restore=qScopeGuard([&]{qputenv("PATH",old);});qputenv("PATH",(dir.filePath("bin")+":/usr/bin").toUtf8());
        Backend b(dir.filePath("data"),nullptr,false);auto *agents=b.agents();auto run=agents->start("Review a loop",{{"name","Master loop"},{"id",1}},"codex");QVERIFY(!run.isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(agents->selected()["state"].toString(),QString("finished"),5000);
        auto report=b.optimizerReport();QCOMPARE(report["reportedInputTokens"].toLongLong(),12LL);QCOMPARE(report["reportedOutputTokens"].toLongLong(),3LL);QCOMPARE(report["usageUnknown"].toInt(),0);
        auto next=agents->start("Check again",{},"codex",run);QVERIFY(!next.isEmpty());QTRY_COMPARE(agents->selected()["state"].toString(),QString("finished"));QCOMPARE(b.optimizerReport()["parentRuns"].toInt(),1);QCOMPARE(agents->selected()["loop"].toMap()["root"].toString(),run);QCOMPARE(agents->selected()["loop"].toMap()["depth"].toInt(),1);QVERIFY(agents->selected()["loop"].toMap()["policyRevision"].toString().size()==64);
        QVERIFY(write(128000));auto large=agents->start("Budget example",{{"name","Example"},{"id",1}},"codex");QVERIFY(!large.isEmpty());QTRY_COMPARE(agents->selected()["state"].toString(),QString("finished"));QVERIFY(agents->start("Again",{},"codex",large).isEmpty());
        auto child=b.addCategory("Scoped","","",0);b.select(child);QVERIFY(b.contextTool("inspect_master_loop",{}).contains("error"));b.select(1);QVERIFY(!b.contextTool("inspect_master_loop",{}).contains("error"));
    }
    void runtimeReportSeparatesPrivateStateAndOutputs() {
        QTemporaryDir dir;Backend b(dir.filePath("data"),nullptr,false);auto report=b.runtimeReport();
        QVERIFY(report["databaseOk"].toBool());QVERIFY(!report["runtimeInsideGit"].toBool());
        auto locations=report["storage"].toList();QCOMPARE(locations.size(),6);
        QSet<QString> paths;for(auto entry:locations){auto row=entry.toMap();QVERIFY(!row["purpose"].toString().isEmpty());paths.insert(row["path"].toString());}QCOMPARE(paths.size(),6);
        QDir().mkpath(dir.filePath("data/.git"));QFile head(dir.filePath("data/.git/HEAD"));QVERIFY(head.open(QIODevice::WriteOnly));head.write("ref: refs/heads/main\n");head.close();
        QVERIFY(b.runtimeReport()["runtimeInsideGit"].toBool());QVERIFY(!b.runtimeReport()["ok"].toBool());
    }
    void activityFeedPagesAndScopesDetails() {
        QTemporaryDir dir;Backend b(dir.path(),nullptr,false);auto child=b.addCategory("Trace","","",0);qint64 oldest=0;
        for(int i=0;i<45;i++){auto id=b.recordEvent(child,i==0?"command.finished":"tool.result",QString("Step %1").arg(i),"payload");if(i==0)oldest=id;}
        QCOMPARE(b.activityPage().size(),40);auto page=b.activityPage(b.activityPage().last().toMap()["id"].toLongLong());QVERIFY(!page.isEmpty());QCOMPARE(b.activityDetail(oldest)["content"].toString(),QString("payload"));
        auto other=b.addCategory("Other","","",0);b.select(other);QVERIFY(b.activityDetail(oldest).contains("error"));b.select(1);QVERIFY(!b.activityDetail(oldest).contains("error"));QVERIFY(!b.activityPage().isEmpty());
    }
    void feedOriginsStayAttachedToMessages() {
        QTemporaryDir dir;int category;QVariantMap first;
        {
            Backend b(dir.path(),nullptr,false);category=b.addCategory("Weather","","",0);
            first=b.identifyFeed(category,{"Favorites","Weather","Chat"},"favorites","Chat",true);
            QVERIFY(!first.isEmpty());QCOMPARE(first,b.identifyFeed(category,{"Favorites","Weather","Chat"},"favorites","Chat",false));
            b.send("First request");
            auto second=b.identifyFeed(category,{"Pinned","Weather","Chat"},"pinned","Chat",true);
            QVERIFY(first["mark"]!=second["mark"]);
            QCOMPARE(b.messages().first().toMap()["origin"].toMap(),first);
            QCOMPARE(b.feedIdentity(),second);
            b.send("Second request, from Pinned");
            Theme theme(dir.path());Translator translator(dir.filePath("language"),dir.path());QQmlApplicationEngine engine;
            engine.rootContext()->setContextProperty("backend",&b);engine.rootContext()->setContextProperty("theme",&theme);engine.rootContext()->setContextProperty("translator",&translator);
            engine.load(QUrl("qrc:/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
            auto *window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());window->setProperty("page",QString("chat"));window->show();QTest::qWait(80);QVERIFY(window->grabWindow().save("/tmp/hotbox-feed-preview.png"));
            auto *feed=window->findChild<QObject*>("chatFeed");QVERIFY(feed);
            std::function<QQuickItem*(QQuickItem*,const QString&)> visual=[&](QQuickItem *item,const QString &name)->QQuickItem*{if(item->objectName()==name)return item;for(auto *child:item->childItems())if(auto *found=visual(child,name))return found;return nullptr;};
            auto *session=visual(window->contentItem(),"sessionDisclosure");QVERIFY(session);QVERIFY(QMetaObject::invokeMethod(session,"clicked"));QTest::qWait(30);
            auto *disclosure=visual(window->contentItem(),"activityDisclosure");QVERIFY(disclosure);QVERIFY(QMetaObject::invokeMethod(disclosure,"clicked"));QTest::qWait(30);
            QVERIFY(!feed->property("detailCache").toMap().isEmpty());QVERIFY(window->grabWindow().save("/tmp/hotbox-activity-preview.png"));
feed->setProperty("followTail",false);b.send("Another update");QTest::qWait(30);QVERIFY(!feed->property("followTail").toBool());

            QVERIFY(b.identifyFeed(category,{"Bad"},"unknown","Chat",true).isEmpty());
            auto other=b.addCategory("Other","","",0);b.select(other);QVERIFY(b.feedIdentity()["mark"]!=first["mark"]);
        }
        {
            Backend b(dir.path(),nullptr,false);b.select(category);
            QCOMPARE(b.messages().first().toMap()["origin"].toMap(),first);
            QCOMPARE(b.feedIdentity()["spread"].toString(),QString("pinned"));
        }
    }
    void persistenceAndArchives() {
        QTemporaryDir dir;
        int parent, child;
        {
            Backend b(dir.path());
            QVERIFY(b.status().isEmpty());
            parent = b.addCategory("Weather", "/tmp/weather", "Hourly weather");
            QVERIFY(parent > 1);
            child = b.addCategory("UI", "/tmp/weather", "Theme support", parent);
            QVERIFY(child > parent);
            b.send("Remember the blue accent");
            QCOMPARE(b.messages().size(), 1);
            b.updateCategory(child, "UI", "/tmp/weather", "Blue accent");
        }
        {
            Backend b(dir.path());
            b.select(child);
            QCOMPARE(b.brief(), QString("Blue accent"));
            QCOMPARE(b.messages().first().toMap()["content"].toString(), QString("Remember the blue accent"));
            b.archive(parent);
            QCOMPARE(b.categories().size(), 1);
            QCOMPARE(b.archivedCategories().size(), 2);
            b.restore(child);
            QCOMPARE(b.categories().size(), 3);
            QCOMPARE(b.children(parent).size(), 1);
            b.archive(1);
            QCOMPARE(b.categories().size(), 3);
        }
    }
    void holdThresholdAndRelease() {
        QTemporaryDir dir;
        Backend b(dir.path());
        QSignalSpy shown(&b, &Backend::showMenu);
        b.hold();
        QTest::qWait(200);
        b.release();
        QTest::qWait(360);
        QCOMPARE(shown.size(), 0);
        QVERIFY(!b.expanded());
        b.hold();
        QTest::qWait(150);
        QVERIFY(!b.expanded());
        b.hold();
        QTRY_VERIFY_WITH_TIMEOUT(b.expanded(), 250);
        QCOMPARE(shown.size(), 1);
        b.release();
        QVERIFY(!b.expanded());
        QTest::qWait(150);
        QCOMPARE(shown.size(), 1);
    }
    void invalidSettingsDoNotReplaceConnection() {
        QTemporaryDir dir;
        Backend b(dir.path());
        QString before = b.endpoint();
        b.saveSettings("http://remote.invalid/v1/chat/completions", "model", "HOTBOX_API_KEY");
        QCOMPARE(b.endpoint(), before);
        QVERIFY(!b.status().isEmpty());
        b.saveSettings("http://127.0.0.1:11434/v1/chat/completions", "local-model", "HOTBOX_API_KEY");
        QCOMPARE(b.model(), QString("local-model"));
    }
    void streamSavesToOriginAndIncludesBrief() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QByteArray requestBody;
        bool replied = false;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection();
            auto buffer = new QByteArray;
            connect(socket, &QObject::destroyed, this, [buffer] { delete buffer; });
            connect(socket, &QTcpSocket::readyRead, this, [&, socket, buffer] {
                *buffer += socket->readAll();
                int end = buffer->indexOf("\r\n\r\n");
                if (end < 0 || replied)
                    return;
                auto header = buffer->left(end);
                int size = 0;
                for (auto line : header.split('\n'))
                    if (line.toLower().startsWith("content-length:"))
                        size = line.mid(15).trimmed().toInt();
                if (buffer->size() - end - 4 < size)
                    return;
                replied = true;
                requestBody = buffer->mid(end + 4, size);
                socket->write(
                    "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n");
                socket->write("data: {\"choices\":[{\"delta\":{\"content\":\"Hello \"}}]}\n\n");
                QTimer::singleShot(60, socket, [socket] {
                    socket->write("data: {\"choices\":[{\"delta\":{\"content\":\"");
                    socket->write(QByteArray::fromHex("e2"));
                });
                QTimer::singleShot(120, socket, [socket] {
                    socket->write(QByteArray::fromHex("9c93"));
                    socket->write("\"}}]}\n\ndata: [DONE]\n\n");
                    socket->disconnectFromHost();
                });
            });
        });
        QTemporaryDir dir;
        Backend b(dir.path());
        int origin = b.addCategory("Project", "/tmp/project", "Use the blue accent");
        b.saveSettings(QString("http://127.0.0.1:%1/v1/chat/completions").arg(server.serverPort()),
                       "test-model", "HOTBOX_TEST_NO_KEY");
        b.send("Build a menu");
        QVERIFY(b.busy());
        b.select(1);
        QTRY_VERIFY_WITH_TIMEOUT(!b.busy(), 3000);
        QVERIFY2(b.status().isEmpty(), qPrintable(b.status()));
        QCOMPARE(b.messages().size(), 0);
        b.select(origin);
        QCOMPARE(b.messages().size(), 2);
        QCOMPARE(b.messages().last().toMap()["content"].toString(), QString::fromUtf8("Hello ✓"));
        auto request = QJsonDocument::fromJson(requestBody).object();
        QVERIFY(request["stream"].toBool());
        QVERIFY(request["messages"].toArray()[0].toObject()["content"].toString().contains(
            "Use the blue accent"));
    }
    void failedConnectionRetainsUserMessage() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        auto port = server.serverPort();
        server.close();
        QTemporaryDir dir;
        Backend b(dir.path());
        b.saveSettings(QString("http://127.0.0.1:%1/v1/chat/completions").arg(port), "test-model",
                       "HOTBOX_TEST_NO_KEY");
        b.send("Keep this message");
        QTRY_VERIFY_WITH_TIMEOUT(!b.busy(), 3000);
        QCOMPARE(b.messages().size(), 1);
        QVERIFY(!b.status().isEmpty());
    }
    void ledgerScopesFocusAndRedaction() {
        QTemporaryDir dir;
        int project, other, child;
        qint64 attached;
        {
            Backend b(dir.path());
            project = b.addCategory("Project", "/tmp/project", "Small project");
            attached = b.recordEvent(project, "command.finished", "Build finished",
                                     "password=example-secret\nCompiled successfully", "/tmp/project");
            QVERIFY(attached > 0);
            other = b.addCategory("Other", "/tmp/other", "Unrelated");
            b.select(project);
            QVERIFY(b.readEvent(attached)["content"].toString().contains("[REDACTED]"));
            QVERIFY(!b.readEvent(attached)["content"].toString().contains("example-secret"));
            auto outsider = b.recordEvent(other, "session.note", "Other secret", "Other content");
            QVERIFY(b.readEvent(outsider).contains("error"));
            QVERIFY(b.contextTool("workspace_overview", {{"category", other}}).contains("error"));
            child = b.focusWorkspace(project, "Menu rendering", "Fix radial layout");
            QVERIFY(child > other);
            QVERIFY(b.brief().contains("Fix radial layout"));
            QVERIFY(b.brief().contains(QString("#%1").arg(attached)));
            QVERIFY(!b.readEvent(attached).contains("error"));
            QVERIFY(b.readEvent(outsider).contains("error"));
            QCOMPARE(b.messages().size(), 0);
            b.select(1);
            auto events = b.timeline();
            QVERIFY(events.size() >= 5);
            for (int i = 1; i < events.size(); ++i)
                QVERIFY(events[i - 1].toMap()["id"].toLongLong() > events[i].toMap()["id"].toLongLong());
            QVERIFY(!events.first().toMap()["created"].toString().isEmpty());
        }
        Backend b(dir.path());
        b.select(1);
        QVERIFY(!b.readEvent(attached).contains("error"));
        b.select(child);
        QVERIFY(b.brief().contains("Fix radial layout"));
    }
    void attachmentsAndBoundedContext() {
        QTemporaryDir dir;
        Backend b(dir.filePath("data"));
        QFile file(dir.filePath("session.jsonl"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"command\":\"build\",\"result\":\"ok\"}\n");
        file.close();
        QVERIFY(b.attachSession(file.fileName()));
        QCOMPARE(b.timeline().first().toMap()["kind"].toString(), QString("session.attached"));
        QFile env(dir.filePath(".env"));
        QVERIFY(env.open(QIODevice::WriteOnly));
        env.write("TOKEN=private\n");
        env.close();
        QVERIFY(!b.attachSession(env.fileName()));
        for (int i = 0; i < 25; ++i)
            b.send(QString(10000, 'x'));
        auto context = b.requestContext();
        int chars = 0;
        for (const auto &v : context)
            chars += v.toObject()["content"].toString().size();
        QVERIFY(chars <= 30000);
        QVERIFY(context.size() <= 13);
        QCOMPARE(b.messages().size(), 25);
        auto all = b.eventPage(1);
        QVERIFY(all.size() > 20);
        auto older = b.eventPage(1, all.last().toMap()["id"].toLongLong());
        QVERIFY(older.isEmpty());
    }
    void managedGitIsolationCheckpointAndPush() {
        QTemporaryDir dir;
        QString source = dir.filePath("source"), remote = dir.filePath("remote.git"),
                work = dir.filePath("work");
        QDir().mkpath(source);
        auto git = [](const QString &cwd, const QStringList &args) {
            QProcess p;
            p.start("git", QStringList{"-C", cwd} + args);
            if (!p.waitForFinished(5000))
                return false;
            return p.exitCode() == 0;
        };
        auto write = [](const QString &path, const QByteArray &data) {
            QFile f(path);
            return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
        };
        QVERIFY(git(source, {"init"}));
        QVERIFY(git(source, {"config", "user.name", "Hotbox Test"}));
        QVERIFY(git(source, {"config", "user.email", "test@example.invalid"}));
        QVERIFY(write(source + "/app.txt", "initial\n"));
        QVERIFY(git(source, {"add", "app.txt"}));
        QVERIFY(git(source, {"commit", "-m", "Initial"}));
        QVERIFY(git(dir.path(), {"init", "--bare", remote}));
        QVERIFY(git(source, {"remote", "add", "origin", remote}));
        QVERIFY(write(source + "/app.txt", "uncommitted source edit\n"));
        auto fork = GitWorkspace::fork(source, work, "hotbox/test");
        QVERIFY2(fork["ok"].toBool(), qPrintable(fork["error"].toString()));
        QFile initial(work + "/app.txt");
        QVERIFY(initial.open(QIODevice::ReadOnly));
        QCOMPARE(initial.readAll(), QByteArray("initial\n"));
        initial.close();
        QVERIFY(write(work + "/app.txt", "focused edit\n"));
        QVERIFY(write(work + "/unselected.txt", "leave alone\n"));
        auto checkpoint = GitWorkspace::checkpoint(work, "hotbox/test", {"app.txt"}, "origin", true);
        QVERIFY2(checkpoint["ok"].toBool(), qPrintable(checkpoint["error"].toString()));
        QVERIFY(checkpoint["pushed"].toBool());
        QFile original(source + "/app.txt");
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), QByteArray("uncommitted source edit\n"));
        QVERIFY(git(remote, {"show-ref", "--verify", "refs/heads/hotbox/test"}));
        auto snapshot = GitWorkspace::snapshot(work);
        QVERIFY(snapshot["tree"].toString().contains("app.txt"));
        QVERIFY(!snapshot["tree"].toString().contains("unselected.txt"));
        QVERIFY(write(work + "/.env", "API_KEY=private\n"));
        QVERIFY(!GitWorkspace::checkpoint(work, "hotbox/test", {".env"}, "", false)["ok"].toBool());
        QVERIFY(
            !GitWorkspace::checkpoint(work, "hotbox/test", {"../source/app.txt"}, "", false)["ok"].toBool());
        QVERIFY(write(work + "/credentials.txt", "password=private\n"));
        QVERIFY(
            !GitWorkspace::checkpoint(work, "hotbox/test", {"credentials.txt"}, "", false)["ok"].toBool());
        QVERIFY(git(work, {"add", "unselected.txt"}));
        QVERIFY(!GitWorkspace::checkpoint(work, "hotbox/test", {"app.txt"}, "", false)["ok"].toBool());
    }
    void externalSessionUpdatesReachResidentApp() {
        QTemporaryDir dir;
        Backend resident(dir.path());
        Backend writer(dir.path());
        QSignalSpy activity(&resident, &Backend::activityChanged);
        QVERIFY(writer.recordEvent(1, "session.note", "External update", "Details") > 0);
        QTRY_VERIFY_WITH_TIMEOUT(activity.size() > 0, 3000);
        QCOMPARE(resident.timeline().first().toMap()["summary"].toString(), QString("External update"));
    }
    void backgroundGitResumesAndPausesOnFailure() {
        QTemporaryDir dir;
        QString source = dir.filePath("source");
        QDir().mkpath(source);
        auto git = [&](const QStringList &args) {
            QProcess p;
            p.start("git", QStringList{"-C", source} + args);
            return p.waitForFinished(5000) && p.exitCode() == 0;
        };
        auto write = [](const QString &path, const QByteArray &data) {
            QFile f(path);
            return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
        };
        QVERIFY(git({"init"}));
        QVERIFY(git({"config", "user.name", "Hotbox Test"}));
        QVERIFY(git({"config", "user.email", "test@example.invalid"}));
        QVERIFY(write(source + "/app.txt", "initial\n"));
        QVERIFY(git({"add", "app.txt"}));
        QVERIFY(git({"commit", "-m", "Initial"}));
        int id;
        QString work;
        {
            Backend b(dir.filePath("data"));
            id = b.addCategory("Project", source, "Test background work");
            b.forkGit();
            QTRY_VERIFY_WITH_TIMEOUT(!b.taskBusy(), 10000);
            QVERIFY(b.gitState()["managed"].toBool());
            work = b.folder();
            QVERIFY(work != source);
            b.configureGit("app.txt", "", true, false);
            QVERIFY(b.gitState()["automatic"].toBool());
        }
        {
            Backend b(dir.filePath("data"));
            b.select(id);
            QVERIFY(b.gitState()["automatic"].toBool());
            QVERIFY(write(work + "/app.txt", "background change\n"));
            b.processBackgroundGit();
            QTRY_VERIFY_WITH_TIMEOUT(!b.taskBusy(), 10000);
            QCOMPARE(b.timeline().first().toMap()["kind"].toString(), QString("git.checkpoint"));
            auto event = b.readEvent(b.timeline().first().toMap()["id"].toLongLong());
            QVERIFY(event["content"].toString().contains("history"));
            b.configureGit("app.txt", "missing-remote", true, true);
            b.processBackgroundGit();
            QTRY_VERIFY_WITH_TIMEOUT(!b.taskBusy(), 10000);
            QVERIFY(!b.gitState()["automatic"].toBool());
            QCOMPARE(b.timeline().first().toMap()["kind"].toString(), QString("git.error"));
        }
    }
    void modelToolLoopUsesOriginalWorkspace() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int requests = 0;
        QJsonObject second;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto socket = server.nextPendingConnection();
            auto buffer = new QByteArray;
            connect(socket, &QObject::destroyed, this, [buffer] { delete buffer; });
            connect(socket, &QTcpSocket::readyRead, this, [&, socket, buffer] {
                *buffer += socket->readAll();
                int end = buffer->indexOf("\r\n\r\n");
                if (end < 0)
                    return;
                int size = 0;
                for (auto line : buffer->left(end).split('\n'))
                    if (line.toLower().startsWith("content-length:"))
                        size = line.mid(15).trimmed().toInt();
                if (buffer->size() - end - 4 < size)
                    return;
                auto body = QJsonDocument::fromJson(buffer->mid(end + 4, size)).object();
                buffer->clear();
                ++requests;
                QJsonObject message;
                if (requests == 1) {
                    socket->write(
                        "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n");
                    QJsonObject first{{"index", 0},
                                      {"id", "call_test"},
                                      {"type", "function"},
                                      {"function", QJsonObject{{"name", "workspace_overview"},
                                                               {"arguments", "{\"category\":"}}}};
                    QJsonObject last{{"index", 0},
                                     {"function", QJsonObject{{"arguments", "2,\"before\":0}"}}}};
                    for (const auto &call : QJsonArray{first, last}) {
                        QJsonObject chunk{
                            {"choices", QJsonArray{QJsonObject{
                                            {"delta", QJsonObject{{"tool_calls", QJsonArray{call}}}}}}}};
                        socket->write("data: " + QJsonDocument(chunk).toJson(QJsonDocument::Compact) +
                                      "\n\n");
                    }
                    socket->write("data: [DONE]\n\n");
                    socket->disconnectFromHost();
                    return;
                }
                second = body;
                message = {{"role", "assistant"}, {"content", "Read the project events."}};
                QByteArray response =
                    QJsonDocument(QJsonObject{{"choices", QJsonArray{QJsonObject{{"message", message}}}}})
                        .toJson(QJsonDocument::Compact);
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: "
                              "close\r\nContent-Length: " +
                              QByteArray::number(response.size()) + "\r\n\r\n" + response);
                socket->disconnectFromHost();
            });
        });
        QTemporaryDir dir;
        Backend b(dir.path());
        int origin = b.addCategory("Project", "/tmp/project", "Origin brief");
        QCOMPARE(origin, 2);
        b.saveSettings(QString("http://127.0.0.1:%1/v1/chat/completions").arg(server.serverPort()), "test",
                       "HOTBOX_TEST_NO_KEY");
        b.send("Read my history");
        b.select(1);
        QTRY_VERIFY_WITH_TIMEOUT(!b.busy(), 4000);
        QCOMPARE(requests, 2);
        auto messages = second["messages"].toArray();
        QCOMPARE(messages.last().toObject()["role"].toString(), QString("tool"));
        QVERIFY(messages.last().toObject()["content"].toString().contains("Origin brief"));
        QCOMPARE(b.messages().size(), 0);
        b.select(origin);
        QCOMPARE(b.messages().last().toMap()["content"].toString(), QString("Read the project events."));
    }
    void segmentEvidenceQueueAndWorkerCompletion() {
        QTemporaryDir dir;
        Backend b(dir.path());
        auto segment = b.segmentAction("create", {{"kind", "web_search"},
                                                  {"title", "Rendering research"},
                                                  {"description", "Compare native rendering options"}});
        QVERIFY2(!segment.contains("error"), qPrintable(segment["error"].toString()));
        QString id = segment["id"].toString();
        QVERIFY(id.startsWith("seg_"));
        QString body = QString(18000, 'x') + "\nA final finding.";
        auto input =
            b.segmentAction("add", {{"id", id}, {"kind", "input"}, {"content", "Find rendering options"}});
        auto output = b.segmentAction("add", {{"id", id},
                                              {"kind", "output"},
                                              {"content", body},
                                              {"source", "https://example.invalid/research"}});
        auto script =
            b.segmentAction("add", {{"id", id}, {"kind", "script"}, {"content", "print('analysis')\n"}});
        QVERIFY(!input.contains("error"));
        QVERIFY(!output.contains("error"));
        QVERIFY(!script.contains("error"));
        QCOMPARE(output["sha256"].toString().size(), 64);
        QCOMPARE(b.segmentAction("ready", {{"id", id}})["state"].toString(), QString("ready"));
        QVERIFY(b.segmentAction("add", {{"id", id}, {"kind", "output"}, {"content", "overwrite"}})
                    .contains("error"));
        auto job = b.segmentAction("claim", {{"worker", "cold-code-test"}, {"seconds", 60}});
        QCOMPARE(job["id"].toString(), id);
        QVERIFY(job["lease"].toString().startsWith("lease_"));
        Backend competing(dir.path());
        QVERIFY(competing.segmentAction("claim", {{"worker", "second-worker"}})["empty"].toBool());
        QVERIFY(b.segmentAction(
                     "finish",
                     {{"id", id}, {"lease", "wrong"}, {"description", "Description"}, {"summary", "Summary"}})
                    .contains("error"));
        QVERIFY(!b.segmentAction("renew", {{"id", id}, {"lease", job["lease"]}, {"seconds", 120}})
                     .contains("error"));
        auto complete = b.segmentAction("finish", {{"id", id},
                                                   {"lease", job["lease"]},
                                                   {"description", "Rendering comparison"},
                                                   {"summary", "Use native Qt rendering."},
                                                   {"content", "{\"choice\":\"Qt\"}"}});
        QCOMPARE(complete["state"].toString(), QString("complete"));
        QCOMPARE(complete["artifacts"].toList().size(), 4);
        QCOMPARE(complete["revisions"].toList().size(), 2);
        QVERIFY(
            b.segmentAction("finish",
                            {{"id", id}, {"lease", job["lease"]}, {"description", "Bad"}, {"summary", "Bad"}})
                .contains("error"));
        QString reconstructed;
        int offset = 0;
        do {
            auto part = b.segmentAction("part", {{"id", output["id"]}, {"offset", offset}});
            QVERIFY(!part.contains("error"));
            reconstructed += part["content"].toString();
            offset = part["nextOffset"].toInt();
        } while (offset >= 0);
        QCOMPARE(reconstructed, body);
        QString throughTool;
        int toolOffset = 0;
        do {
            auto part = b.contextTool("read_segment_part", {{"id", output["id"]}, {"offset", toolOffset}});
            throughTool += part["content"].toString();
            toolOffset = part["nextOffset"].toInt();
        } while (toolOffset >= 0);
        QCOMPARE(throughTool, body);
        auto index = b.contextTool("read_segment", {{"id", id}, {"offset", 0}});
        QCOMPARE(index["artifacts"].toList().size(), 4);
        QCOMPARE(index["nextArtifactOffset"].toInt(), -1);

        auto matches = b.segmentPage("complete", "rendering");
        QCOMPARE(matches.size(), 1);
        QVERIFY(!matches.first().toMap().contains("artifacts"));
    }
    void segmentRetryExpiredLeaseAndImmutableOriginals() {
        QTemporaryDir dir;
        Backend b(dir.path());
        auto s = b.segmentAction("create", {{"kind", "code"}, {"title", "A task"}});
        QString id = s["id"].toString();
        auto original = b.segmentAction(
            "add", {{"id", id}, {"kind", "output"}, {"content", "password=private\nOriginal evidence"}});
        QVERIFY(!original.contains("error"));
        b.segmentAction("ready", {{"id", id}});
        auto first = b.segmentAction("claim", {{"worker", "first"}, {"seconds", 1}});
        QTest::qWait(1200);
        auto second = b.segmentAction("claim", {{"worker", "recovered"}});
        QCOMPARE(second["id"].toString(), id);
        QVERIFY(first["lease"] != second["lease"]);
        QCOMPARE(second["attempts"].toInt(), 2);
        QVERIFY(b.segmentAction("fail", {{"id", id}, {"lease", first["lease"]}, {"error", "Stale worker"}})
                    .contains("error"));
        QCOMPARE(b.segmentAction("fail", {{"id", id},
                                          {"lease", second["lease"]},
                                          {"error", "Worker failed with diagnostic details"}})["state"]
                     .toString(),
                 QString("failed"));
        QCOMPARE(b.segmentAction("retry", {{"id", id}})["state"].toString(), QString("ready"));
        auto done = b.segmentAction("process", {});
        QCOMPARE(done["state"].toString(), QString("complete"));
        QVERIFY(done["summary"].toString().contains("Original evidence"));
        auto part = b.segmentAction("part", {{"id", original["id"]}});
        QVERIFY(part["content"].toString().contains("[REDACTED]"));
        QVERIFY(!part["content"].toString().contains("private"));
        Backend reopened(dir.path());
        QCOMPARE(reopened.segmentAction("read", {{"id", id}})["state"].toString(), QString("complete"));
    }
    void segmentCompositionScopeAndFocusedReferences() {
        QTemporaryDir dir;
        Backend b(dir.path());
        int project = b.addCategory("Project", "/tmp/project", "Research"),
            other = b.addCategory("Other", "/tmp/other", "Other context");
        auto hidden = b.segmentAction("create", {{"kind", "analysis"}, {"title", "Other workspace"}});
        b.select(project);
        b.send("Remember this original chat");
        auto fromChat = b.segmentAction("from_chat", {{"title", "Chat evidence"}});
        QString id = fromChat["id"].toString();
        QCOMPARE(fromChat["artifacts"].toList().size(), 1);
        QVERIFY(b.segmentAction("read", {{"id", hidden["id"]}}).contains("error"));
        b.segmentAction("ready", {{"id", id}});
        auto processed = b.segmentAction("process");
        QCOMPARE(processed["state"].toString(), QString("complete"));
        auto bundle = b.segmentAction("compose", {{"title", "Research bundle"}, {"ids", QStringList{id}}});
        QCOMPARE(bundle["links"].toList().size(), 1);
        QCOMPARE(bundle["artifacts"].toList().size(), 0);
        b.segmentAction("ready", {{"id", bundle["id"]}});
        auto combined = b.segmentAction("process");
        QVERIFY(combined["summary"].toString().contains(id));
        int focused = b.focusWorkspace(project, "Focused reader", "Use the research");
        QVERIFY(focused > other);
        QVERIFY(b.brief().contains(id));
        auto allowed = b.segmentAction("read", {{"id", id}});
        QVERIFY(!allowed.contains("error"));
        auto part = b.segmentAction("part", {{"id", allowed["artifacts"].toList().first().toMap()["id"]}});
        QVERIFY(part["content"].toString().contains("Remember this original chat"));
        QVERIFY(b.segmentAction("read", {{"id", hidden["id"]}}).contains("error"));
        QVERIFY(b.segmentAction("add", {{"id", id}, {"kind", "notes"}, {"content", "Bad mutation"}})
                    .contains("error"));
        auto tool = b.contextTool("find_segments", {{"state", "complete"}, {"query", "Chat evidence"}});
        QCOMPARE(tool["segments"].toList().size(), 2);
        QVERIFY(b.contextTool("read_segment", {{"id", hidden["id"]}}).contains("error"));
    }
    void segmentRejectsOversizeWithoutTruncation() {
        QTemporaryDir dir;
        Backend b(dir.path());
        auto s = b.segmentAction("create", {{"kind", "experiment"}, {"title", "Evidence limits"}});
        QString id = s["id"].toString();
        QVERIFY(b.segmentAction("add",
                                {{"id", id}, {"kind", "output"}, {"content", QString(1024 * 1024 + 1, 'x')}})
                    .contains("error"));
        QCOMPARE(b.segmentAction("read", {{"id", id}})["artifacts"].toList().size(), 0);
        QVERIFY(b.segmentAction("ready", {{"id", id}}).contains("error"));
    }
    void architectureReusesLibrariesAndPreservesBelongings() {
        QTemporaryDir dir;
        QString data = dir.filePath("private"), musicPath, source;
        int first, second;
        {
            Backend b(data);
            auto initialized = b.architectureAction("init");
            QVERIFY2(initialized["ok"].toBool(), qPrintable(initialized["error"].toString()));
            QVERIFY(b.architectureAction("init")["reused"].toBool());
            QString runtime = initialized["runtimeRoot"].toString();
            QVERIFY(!QFileInfo::exists(runtime + "/media"));
            auto snapshot = GitWorkspace::snapshot(initialized["toolRoot"].toString());
            QVERIFY(snapshot["ok"].toBool());
            QVERIFY(snapshot["tree"].toString().contains("templates/media-v1.json"));
            first = b.addCategory("Player", "", "", 1);
            second = b.addCategory("Another app", "", "", 1);
            b.select(first);
            QVariantMap args{{"capabilities", QStringList{"music", "album-covers"}},
                             {"artist", "Example Band"},
                             {"album", "First Album"},
                             {"genre", "Rock"}};
            auto preview = b.architectureAction("plan", args);
            QVERIFY2(preview["ok"].toBool(), qPrintable(preview["error"].toString()));
            source = preview["source"].toString();
            QVERIFY(!QFileInfo::exists(source));
            auto created = b.architectureAction("ensure", args);
            QVERIFY2(created["ok"].toBool(), qPrintable(created["error"].toString()));
            QCOMPARE(b.folder(), source);
            musicPath = created["entity"].toString() + "/songs";
            QVERIFY(QFileInfo::exists(musicPath));
            QVERIFY(QFileInfo::exists(runtime + "/media/pictures/bands"));
            QVERIFY(!QFileInfo::exists(runtime + "/media/video"));
            QFile song(musicPath + "/private-song.ogg");
            QVERIFY(song.open(QIODevice::WriteOnly));
            song.write("private-song-bytes");
            song.close();
            QVERIFY(b.architectureAction("ensure", args)["ok"].toBool());
            QCOMPARE(b.architecture()["genres"].toList().size(), 1);
            b.select(second);
            QCOMPARE(b.architecture()["attachments"].toList().size(), 0);
            auto reused = b.architectureAction("ensure", args);
            QVERIFY(reused["ok"].toBool());
            QCOMPARE(reused["entity"], created["entity"]);
            QCOMPARE(b.architecture()["genres"].toList().size(), 1);
            auto musicOnly = args;
            musicOnly["genre"] = "Jazz";
            QVERIFY(b.architectureAction("ensure", musicOnly)["ok"].toBool());
            QCOMPARE(b.architecture()["genres"].toList().size(), 2);
            auto sourceTree = GitWorkspace::snapshot(initialized["toolRoot"].toString());
            QVERIFY(!sourceTree["status"].toString().contains("private-song"));
            QFile manifest(source + "/project.json");
            QVERIFY(manifest.open(QIODevice::ReadOnly));
            auto bytes = manifest.readAll();
            QVERIFY(!bytes.contains(runtime.toUtf8()));
            QVERIFY(!bytes.contains("Example Band"));
            QVERIFY(song.open(QIODevice::ReadOnly));
            QCOMPARE(song.readAll(), QByteArray("private-song-bytes"));
            b.archive(first);
            QVERIFY(QFileInfo::exists(musicPath + "/private-song.ogg"));
        }
        Backend reopened(data);
        reopened.select(second);
        QVERIFY(reopened.architecture()["initialized"].toBool());
        QCOMPARE(reopened.architecture()["genres"].toList().size(), 2);
        QVERIFY(QFileInfo::exists(musicPath + "/private-song.ogg"));
    }
    void architectureRejectsUnsafePathsAndScopesModelTools() {
        QTemporaryDir dir;
        Backend b(dir.filePath("private"));
        QVERIFY(b.architectureAction("init", {{"toolRoot", dir.filePath("same")},
                                              {"runtimeRoot", dir.filePath("same/child")}})
                    .contains("error"));
        QVERIFY(!QFileInfo::exists(dir.filePath("same")));
        auto initialized = b.architectureAction("init");
        QVERIFY(initialized["ok"].toBool());
        int first = b.addCategory("First", "", "", 1), second = b.addCategory("Second", "", "", 1);
        QVariantMap args{{"category", first},
                         {"capabilities", QStringList{"music"}},
                         {"artist", "Band"},
                         {"album", "Album"},
                         {"genre", "Genre"}};
        b.select(first);
        QVERIFY(b.architectureAction("init").contains("error"));
        QVERIFY(b.architectureAction("ensure", args)["ok"].toBool());
        b.select(second);
        auto scoped = b.contextTool("read_architecture", {}, second);
        QVERIFY(scoped["attachments"].toList().isEmpty());
        QVERIFY(!scoped.contains("runtimeRoot"));
        QVERIFY(b.contextTool("plan_storage", args, second).contains("error"));
        QVERIFY(b.contextTool("plan_storage", args, 1)["ok"].toBool());
        QVERIFY(b.contextTool("ensure_storage", args, second).contains("error"));
        auto unsafe = args;
        unsafe["artist"] = "../../outside";
        QVERIFY(b.architectureAction("ensure", unsafe).contains("error"));
        unsafe = args;
        unsafe["capabilities"] = QStringList{"unknown"};
        QVERIFY(b.architectureAction("ensure", unsafe).contains("error"));
        unsafe = args;
        unsafe["artist"] = "";
        QVERIFY(b.architectureAction("ensure", unsafe).contains("error"));
        QString runtime = initialized["runtimeRoot"].toString();
        QVERIFY(QDir().mkpath(dir.filePath("outside")));
        QVERIFY(QFile::link(dir.filePath("outside"), runtime + "/media/sounds"));
        QVERIFY(b.architectureAction("ensure", {{"capabilities", QStringList{"sounds"}}}).contains("error"));
        QVERIFY(QDir(dir.filePath("outside")).isEmpty());
        int linked = b.addCategory("Existing folder", dir.filePath("unrelated"), "", 1);
        b.select(linked);
        QVERIFY(b.architectureAction("ensure", args).contains("error"));
        b.select(1);
        auto master = b.contextTool("read_architecture", {}, 1);
        QVERIFY(!master["attachments"].toList().isEmpty());
        QVERIFY(master["genres"].toList().size() == 1);
    }
    void architectureExtendsTemplateBeforeCreatingDirectories() {
        QTemporaryDir dir;
        Backend b(dir.filePath("private"));
        auto initialized = b.architectureAction(
            "init", {{"toolRoot", dir.filePath("tool")}, {"runtimeRoot", dir.filePath("user")}});
        QVERIFY(initialized["ok"].toBool());
        QVERIFY(b.architectureAction("init")["reused"].toBool());
        const QString runtime = initialized["runtimeRoot"].toString();
        QVariantMap definition{{"id", "documents"},
                               {"path", "libraries/documents"},
                               {"description", "Reusable document library"}};
        auto registered = b.architectureAction("define", definition);
        QVERIFY2(registered["ok"].toBool(), qPrintable(registered["error"].toString()));
        QVERIFY(b.architectureAction("define", definition)["reused"].toBool());
        QVERIFY(!QFileInfo::exists(runtime + "/libraries"));
        auto bad = definition;
        bad["path"] = "libraries/../../escape";
        QVERIFY(b.architectureAction("define", bad).contains("error"));
        bad["path"] = "libraries/other";
        QVERIFY(b.architectureAction("define", bad).contains("error"));
        int project = b.addCategory("Reader", "", "", 1);
        b.select(project);
        QVERIFY(b.architectureAction("define", definition).contains("error"));
        auto args = QJsonObject{{"capabilities", QJsonArray{"documents"}}}.toVariantMap();
        auto plan = b.architectureAction("plan", args);
        QVERIFY2(plan["ok"].toBool(), qPrintable(plan["error"].toString()));
        QVERIFY(!QFileInfo::exists(runtime + "/libraries/documents"));
        QVERIFY(b.architectureAction("ensure", args)["ok"].toBool());
        QVERIFY(QFileInfo::exists(runtime + "/libraries/documents"));
        // An edited source file is not replaced by repeat provisioning.
        QFile source(b.folder() + "/src/.gitkeep");
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("user source content");
        source.close();
        QVERIFY(b.architectureAction("ensure", args).contains("error"));
        QVERIFY(source.open(QIODevice::ReadOnly));
        QCOMPARE(source.readAll(), QByteArray("user source content"));
    }
    void notesTasksRecoverAndFinishWithoutAWindow() {
        const QByteArray previous = qgetenv("HOTBOX_CORE_BINARY");
        const QString helper = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../hotbox-core");
        QVERIFY(QFileInfo(helper).isExecutable());
        qputenv("HOTBOX_CORE_BINARY", helper.toUtf8());
        QTemporaryDir dir;
        QString id;
        {
            Backend b(dir.path());
            auto *service = b.tasks();
            QVERIFY(service);
            QTest::qWait(100);
            QTRY_VERIFY(!service->busy());
            QVERIFY2(service->available(), qPrintable(service->problem()));
            service->start("Weekend project", "Measure the wall.\nReuse the lamp.\nMeasure the wall.", 1);
            QTRY_VERIFY(!service->selected().isEmpty());
            QTRY_VERIFY(!service->busy());
            id = service->selected()["id"].toString();
            service->control("pause");
            QTRY_COMPARE(service->selected()["status"].toString(), QString("paused"));
        }
        {
            Backend b(dir.path());
            auto *service = b.tasks();
            QTRY_COMPARE(service->jobs().size(), 1);
            QTRY_VERIFY(!service->busy());
            service->select(id);
            QTRY_COMPARE(service->selected()["status"].toString(), QString("paused"));
            QTRY_VERIFY(!service->busy());
            service->control("resume");
            QTRY_COMPARE_WITH_TIMEOUT(service->selected()["status"].toString(), QString("complete"), 10000);
            QFile result(service->selected()["result_path"].toString());
            QVERIFY(result.open(QIODevice::ReadOnly));
            const auto text = result.readAll();
            QVERIFY(text.contains("2 unique notes"));
            QCOMPARE(text.count("Measure the wall."), 1);
            QVERIFY(service->problem().isEmpty());
        }
        if (previous.isNull()) qunsetenv("HOTBOX_CORE_BINARY"); else qputenv("HOTBOX_CORE_BINARY", previous);
    }
    void taskScreenRunsFromFormAndSurvivesWindowHide() {
        const QByteArray previous = qgetenv("HOTBOX_CORE_BINARY");
        qputenv("HOTBOX_CORE_BINARY", QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../hotbox-core").toUtf8());
        QTemporaryDir dir;
        {
            Backend b(dir.path());
            Theme theme(dir.path());
            Translator translator(dir.filePath("language"), dir.path());
            QQmlApplicationEngine engine;
            QStringList warnings;
            connect(&engine, &QQmlEngine::warnings, this, [&](const QList<QQmlError> &errors) {for (const auto &e : errors) warnings << e.toString();});
            engine.rootContext()->setContextProperty("backend", &b);
            engine.rootContext()->setContextProperty("theme", &theme);
            engine.rootContext()->setContextProperty("translator", &translator);
            engine.load(QUrl("qrc:/Main.qml"));
            QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
            auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            auto *view = win->findChild<QObject *>("tasksView");
            QVERIFY(view);
            win->show();
            QTest::qWait(100);
            QTRY_VERIFY(!b.tasks()->busy());
            QVERIFY(QMetaObject::invokeMethod(view, "newTask"));
            auto *title = win->findChild<QObject *>("taskTitle");
            auto *notes = win->findChild<QObject *>("taskNotes");
            auto *start = win->findChild<QObject *>("startTaskButton");
            QVERIFY(title && notes && start);
            title->setProperty("text", "Reading corner");
            notes->setProperty("text", "Measure the wall.\nPick a chair.");
            QTRY_VERIFY(start->property("enabled").toBool());
            QVERIFY(QMetaObject::invokeMethod(start, "clicked"));
            win->hide();
            QTRY_COMPARE_WITH_TIMEOUT(b.tasks()->selected()["status"].toString(), QString("complete"), 10000);
            QVERIFY(QFileInfo::exists(b.tasks()->selected()["result_path"].toString()));
            win->show();
            QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        }
        if (previous.isNull()) qunsetenv("HOTBOX_CORE_BINARY"); else qputenv("HOTBOX_CORE_BINARY", previous);
    }
    void explicitRecordingsPauseOnRestartAndKeepPrivateDrafts() {
        QTemporaryDir dir;
        QString demo;
        {
            Backend b(dir.path());auto *r=b.recorder();
            QVERIFY(r->start("Morning routine"));demo=r->current()["id"].toString();
            QVERIFY(r->addUrl("https://example.org/news?q=private&token=never-save#login"));
            r->action("browser","Open browser");r->action("keyboard_text","never save typed keys");
            QCOMPARE(r->current()["count"].toInt(),2);
            r->togglePause();QVERIFY(!r->addNote("not recorded"));
            r->togglePause();
            {Backend reader(dir.path(),nullptr,false);QCOMPARE(r->current()["state"].toString(),QString("recording"));}
        }
        {
            Backend b(dir.path());auto *r=b.recorder();QCOMPARE(r->current()["state"].toString(),QString("paused"));
            r->stop();QVERIFY(r->makeDraft("Morning helper"));QCOMPARE(r->draft()["state"].toString(),QString("draft"));
            QVERIFY(!r->draft()["enabled"].toBool());
            QVERIFY(!QJsonDocument(QJsonObject::fromVariantMap(r->current())).toJson().contains("never-save"));
        }
        {
            Backend b(dir.path());b.recorder()->select(demo);QCOMPARE(b.recorder()->draft()["name"].toString(),QString("Morning helper"));
        }
    }
    void browserImportsOmitValuesAndRejectUnsupportedActions() {
        QTemporaryDir dir;Backend b(dir.path());auto *r=b.recorder();
        auto write=[&](const QJsonObject &object){QFile file(dir.filePath("browser.json"));if(!file.open(QIODevice::WriteOnly))return false;file.write(QJsonDocument(object).toJson());return true;};
        QJsonArray steps{QJsonObject{{"kind","navigate"},{"url","https://example.org/search?token=never-save#private"}},
            QJsonObject{{"kind","input_field"},{"url","https://example.org/search"},{"target","Search"},{"value","private-typed-value"},{"cookies","secret-cookie"}},
            QJsonObject{{"kind","input_field"},{"url","https://example.org/login"},{"target","Password"},{"value","typed-password"}}};
        QVERIFY(write({{"format","hotbox-demonstration-v1"},{"steps",steps},{"cookies","secret-cookie"}}));
        QVERIFY2(r->importBrowser(QUrl::fromLocalFile(dir.filePath("browser.json"))),qPrintable(r->problem()));
        QCOMPARE(r->current()["count"].toInt(),2);QCOMPARE(r->current()["state"].toString(),QString("stopped"));
        QVERIFY(r->makeDraft("Search helper"));QCOMPARE(r->draft()["parameters"].toStringList().size(),1);
        const auto payload=QJsonDocument(QJsonObject::fromVariantMap(r->current())).toJson()+QJsonDocument(QJsonObject::fromVariantMap(r->draft())).toJson();
        for(const auto &secret:QList<QByteArray>{"never-save","private-typed-value","secret-cookie","typed-password"})QVERIFY(!payload.contains(secret));
        const int count=r->history().size();steps.append(QJsonObject{{"kind","execute_script"},{"script","untrusted"}});
        QVERIFY(write({{"format","hotbox-demonstration-v1"},{"steps",steps}}));QVERIFY(!r->importBrowser(QUrl::fromLocalFile(dir.filePath("browser.json"))));QCOMPARE(r->history().size(),count);
        QVERIFY(Demonstrations::safeUrl("https://user:secret@example.org/").isEmpty());
        QCOMPARE(Demonstrations::safeUrl("https://example.org/profile/alice"),QString("https://example.org/"));
    }
    void headlessAgentsPersistOutputAndStatus() {
        QTemporaryDir dir;QDir().mkpath(dir.filePath("bin"));
        QFile fake(dir.filePath("bin/codex"));QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\ncat > received.txt\nprintf 'mock output\\n'\nsleep 0.1\n");fake.close();QVERIFY(fake.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        const auto oldPath=qgetenv("PATH");auto restore=qScopeGuard([&]{qputenv("PATH",oldPath);});qputenv("PATH",(dir.filePath("bin")+":/usr/bin").toUtf8());
        QString id;
        {
            Backend b(dir.filePath("data"),nullptr,false);auto *agents=b.agents();
            QVERIFY(agents->start("",{{"name","Example"}},"codex").isEmpty());
            id=agents->start("Explain this project",{{"name","<img src='https://invalid'>"},{"action","category"},{"id",1}},"codex");QVERIFY(!id.isEmpty());
            QTRY_COMPARE_WITH_TIMEOUT(agents->selected()["state"].toString(),QString("finished"),5000);
            QVERIFY(agents->selected()["output"].toString().contains("mock output"));
            Theme theme(dir.path());Translator translator(dir.filePath("language"),dir.path());
            QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("backend",&b);engine.rootContext()->setContextProperty("theme",&theme);engine.rootContext()->setContextProperty("translator",&translator);
            engine.load(QUrl("qrc:/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
            auto *window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());window->setProperty("page",QString("agents"));window->show();QTest::qWait(50);QVERIFY(window->grabWindow().save("/tmp/hotbox-agents-preview.png"));

            QFile state(dir.filePath("data/agent-runs/status.json"));QVERIFY(state.open(QIODevice::ReadOnly));auto jobs=QJsonDocument::fromJson(state.readAll()).object()["jobs"].toArray();QCOMPARE(jobs.size(),1);QCOMPARE(jobs[0].toObject()["title"].toString(),QString("<img src='https://invalid'>"));
            QFile input(agents->selected()["path"].toString()+"/received.txt");QVERIFY(input.open(QIODevice::ReadOnly));QVERIFY(input.readAll().contains("Explain this project"));
            auto next=agents->start("Continue",{},"codex",id);QVERIFY(!next.isEmpty());
            QTRY_COMPARE_WITH_TIMEOUT(agents->selected()["state"].toString(),QString("finished"),5000);
            agents->dismiss(id);agents->dismiss(next);state.close();QVERIFY(state.open(QIODevice::ReadOnly));QVERIFY(QJsonDocument::fromJson(state.readAll()).object()["jobs"].toArray().isEmpty());
            QSignalSpy open(&b,&Backend::showAgent);b.openAgent(id,"prompt");QCOMPARE(open.size(),1);QCOMPARE(agents->selected()["id"].toString(),id);
        }
        {
            Backend b(dir.filePath("data"),nullptr,false);QVERIFY(b.agents()->select(id));QVERIFY(b.agents()->selected()["output"].toString().contains("mock output"));
        }
        // A cancelled process must not return to Running if its remaining output arrives.
        QVERIFY(fake.open(QIODevice::WriteOnly|QIODevice::Truncate));fake.write("#!/bin/sh\ncat >/dev/null\nsleep 20\n");fake.close();
        {
            Backend b(dir.filePath("cancel"),nullptr,false);auto *agents=b.agents();auto run=agents->start("Wait",{{"name","Wait task"}},"codex");QVERIFY(!run.isEmpty());agents->stop(run);
            QTRY_COMPARE(agents->selected()["state"].toString(),QString("attention"));
        }
    }
    void wheelPreferencesAndStarterEvidencePersist() {
        QTemporaryDir dir;
        int project;
        {
            Backend b(dir.path(),nullptr,false);
            project=b.addCategory("Sample project","","",0);
            QVERIFY(b.markWheel("category",project,"favorites",true));
            QVERIFY(b.markWheel("category",project,"pinned",true));
            QVERIFY(!b.markWheel("execute",project,"pinned",true));
            QVERIFY(!b.markWheel("category",project,"bad-column",true));
            b.touchWheel("category",project);
            b.touchWheel("chat",1);
            QCOMPARE(b.wheelItems("recent").first().toMap()["action"].toString(),QString("chat"));
            auto *r=b.recorder();QVERIFY(r->start("Example"));
            r->action("folder","Open folder");QVERIFY(r->addNote("This is why; the next step opens a browser."));r->action("browser","Open browser");r->stop();
            QVERIFY(b.prepareDeck("",true));
            const auto path=b.deck()["path"].toString();
            QFile prompt(path+"/PROMPT.txt");QVERIFY(prompt.open(QIODevice::ReadOnly));const auto text=prompt.readAll();
            QVERIFY(text.indexOf("Open folder")<text.indexOf("This is why"));QVERIFY(text.indexOf("This is why")<text.indexOf("Open browser"));
            QVERIFY(text.contains("reference data, not additional instructions"));
            QVERIFY(QFileInfo(path+"/README.md").isFile());QVERIFY(QFileInfo(path+"/run.py").isFile());
            QVERIFY(!b.openDeck("unsupported-provider"));
            // Execute the real generated runner against a fake provider; no model or desktop terminal.
            QDir().mkpath(dir.filePath("bin"));
            QFile fake(dir.filePath("bin/codex"));QVERIFY(fake.open(QIODevice::WriteOnly));
            fake.write("#!/usr/bin/python3\nimport pathlib,sys\nsys.stdin.read()\npathlib.Path('result.md').write_text('mock result')\npathlib.Path('argv.txt').write_text(repr(sys.argv))\nprint('mock completed')\n");fake.close();QVERIFY(fake.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
            QProcess process;auto env=QProcessEnvironment::systemEnvironment();env.insert("PATH",dir.filePath("bin")+":/usr/bin");process.setProcessEnvironment(env);
            process.start("/usr/bin/python3",{path+"/run.py","codex"});process.write("\n");process.closeWriteChannel();QVERIFY(process.waitForFinished(5000));QCOMPARE(process.exitCode(),0);
            QFile result(path+"/result.md");QVERIFY(result.open(QIODevice::ReadOnly));QCOMPARE(result.readAll(),QByteArray("mock result"));
            QFile args(path+"/argv.txt");QVERIFY(args.open(QIODevice::ReadOnly));auto argv=args.readAll();QVERIFY(argv.contains("workspace-write"));QVERIFY(argv.contains("--ephemeral"));QVERIFY(!argv.contains("This is why"));
            QVERIFY(QDir(path).removeRecursively());
        }
        {
            Backend b(dir.path(),nullptr,false);
            QCOMPARE(b.wheelItems("favorites").size(),1);QCOMPARE(b.wheelItems("pinned").size(),1);
            QVERIFY(b.markWheel("category",project,"favorites",false));QVERIFY(b.wheelItems("favorites").isEmpty());
            b.archive(project);QVERIFY(b.wheelItems("pinned").isEmpty());
        }
    }
    void mouseWheelDirectionsSpreadsAndNotes() {
        QTemporaryDir dir;Backend b(dir.path(),nullptr,false);Theme theme(dir.path());Translator translator(dir.filePath("language"),dir.path());
        QQmlApplicationEngine engine;QStringList warnings;
        connect(&engine,&QQmlEngine::warnings,this,[&](const QList<QQmlError>&errors){for(const auto&e:errors)warnings<<e.toString();});
        engine.rootContext()->setContextProperty("backend",&b);engine.rootContext()->setContextProperty("theme",&theme);engine.rootContext()->setContextProperty("translator",&translator);
        engine.load(QUrl("qrc:/Main.qml"));QVERIFY2(!engine.rootObjects().isEmpty(),qPrintable(warnings.join('\n')));
        auto *win=qobject_cast<QQuickWindow*>(engine.rootObjects().first());auto *wheel=win->findChild<QQuickWindow*>("hotboxOverlay");QVERIFY(wheel);
        wheel->setProperty("anchorX",500);wheel->setProperty("anchorY",350);b.preview();QVERIFY(QMetaObject::invokeMethod(wheel,"resetMenu"));wheel->show();QTest::qWait(220);
        QVERIFY(wheel->grabWindow().save("/tmp/hotbox-wheel-preview.png"));
        QTest::keyClick(wheel,Qt::Key_6,Qt::KeypadModifier);QCOMPARE(wheel->property("groupIndex").toInt(),1);
        QTest::keyClick(wheel,Qt::Key_4);QCOMPARE(wheel->property("groupIndex").toInt(),1);
        QTest::keyClick(wheel,Qt::Key_Left,Qt::KeypadModifier);QCOMPARE(wheel->property("groupIndex").toInt(),0);
        QTest::keyClick(wheel,Qt::Key_7,Qt::KeypadModifier);
        QTest::keyClick(wheel,Qt::Key_Period,Qt::KeypadModifier);QVERIFY(!wheel->property("contextItem").toMap().isEmpty());
        wheel->setProperty("contextItem",QVariantMap());QVERIFY(QMetaObject::invokeMethod(wheel,"resetMenu"));
        QTest::keyClick(wheel,Qt::Key_7,Qt::KeypadModifier);QTest::keyClick(wheel,Qt::Key_0,Qt::KeypadModifier);
        QCOMPARE(wheel->property("entryMode").toString(),QString("agent"));QCOMPARE(wheel->property("agentTarget").toMap()["action"].toString(),QString("task"));
        QTest::keyClick(wheel,Qt::Key_Escape);QVERIFY(QMetaObject::invokeMethod(wheel,"resetMenu"));
        // First press arms Tools (position 3); a repeated key event cannot open it.
        QTest::keyClick(wheel,Qt::Key_3,Qt::KeypadModifier);QCOMPARE(wheel->property("selectedIndex").toInt(),3);QCOMPARE(wheel->property("currentParent").toInt(),0);
        QKeyEvent repeat(QEvent::KeyPress,Qt::Key_3,Qt::KeypadModifier,"3",true);QCoreApplication::sendEvent(wheel,&repeat);QCOMPARE(wheel->property("currentParent").toInt(),0);
        QTest::keyClick(wheel,Qt::Key_3,Qt::KeypadModifier);QCOMPARE(wheel->property("currentParent").toInt(),-1);
        QTest::keyClick(wheel,Qt::Key_5,Qt::KeypadModifier);QTest::keyClick(wheel,Qt::Key_5,Qt::KeypadModifier);QCOMPARE(wheel->property("currentParent").toInt(),0);
        QTest::keyClick(wheel,Qt::Key_Slash,Qt::KeypadModifier);QCOMPARE(b.recorder()->current()["state"].toString(),QString("recording"));
        b.release();QVERIFY(b.expanded());
        b.recorder()->action("folder","Open folder");
        QTest::keyClick(wheel,Qt::Key_Plus,Qt::KeypadModifier);QCOMPARE(wheel->property("entryMode").toString(),QString("note"));
        auto *note=wheel->findChild<QObject*>("wheelNote");QVERIFY(note);note->setProperty("text",QString("Use this folder"));QTest::keyClick(wheel,Qt::Key_Enter);
        QCOMPARE(wheel->property("entryMode").toString(),QString());QCOMPARE(b.recorder()->current()["count"].toInt(),2);
        b.recorder()->togglePause();QVERIFY(!b.recorder()->addNote("Excluded while paused"));
        b.recorder()->action("browser","Excluded while paused");QCOMPARE(b.recorder()->current()["count"].toInt(),2);
        QTest::keyClick(wheel,Qt::Key_Enter);QCOMPARE(b.recorder()->current()["state"].toString(),QString("stopped"));QCOMPARE(wheel->property("entryMode").toString(),QString("deck"));
        QVERIFY(QDir(b.deck()["path"].toString()).removeRecursively());
        wheel->setProperty("entryMode",QString());QVERIFY(QMetaObject::invokeMethod(wheel,"resetMenu"));
        QTest::keyClick(wheel,Qt::Key_Enter);QCOMPARE(wheel->property("entryMode").toString(),QString("prompt"));
        note->setProperty("text",QString("Make a small sample"));QTest::keyClick(wheel,Qt::Key_Enter);QCOMPARE(wheel->property("entryMode").toString(),QString("deck"));
        QVERIFY(QDir(b.deck()["path"].toString()).removeRecursively());b.closeMenu();QVERIFY(!b.expanded());
        QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join('\n')));
    }
    void qmlLoadsAndCenterOpensMaster() {
        QTemporaryDir dir;
        Backend b(dir.path());
        Theme theme(dir.path());
        Translator translator(dir.filePath("language"), dir.path());
        QQmlApplicationEngine engine;
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, this, [&](const QList<QQmlError> &errors) {
            for (auto e : errors)
                warnings << e.toString();
        });
        engine.rootContext()->setContextProperty("backend", &b);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.rootContext()->setContextProperty("translator", &translator);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(win->findChild<QObject *>("architectureView"));
        QVERIFY(b.architectureAction("init")["ok"].toBool());
        int project = b.focusWorkspace(1, "Sample project", "Reusable media storage");
        QCOMPARE(b.folder(), QString(""));
        QVERIFY(project > 1);
        QVERIFY(b.architectureAction("ensure", {{"capabilities", QStringList{"pictures"}}})["ok"].toBool());
        win->setProperty("page", "architecture");
        win->show();
        QTest::qWait(100);
        b.select(1);
        auto *overlay = win->findChild<QQuickWindow *>("hotboxOverlay");
        QVERIFY(overlay);
        overlay->setProperty("anchorX", 500);
        overlay->setProperty("anchorY", 350);
        b.preview();
        overlay->show();
        QTest::qWait(200);
        QSignalSpy chat(&b, &Backend::showChat);
        QTest::mouseDClick(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(500, 350));
        QCOMPARE(chat.size(), 1);
        QCOMPARE(b.currentId(), 1);
        QVERIFY(!b.expanded());
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
};
int main(int argc, char **argv) {
    qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Material");
    HotboxTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "hotbox_tests.moc"
