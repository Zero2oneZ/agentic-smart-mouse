import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
 id: root
 property var owner
 property var rows: []
 property var detail: ({})
 property int detailId: 0
 spacing: 12
 function refresh() { rows=backend.timeline }
 Component.onCompleted: refresh()
 Connections { target: backend; function onActivityChanged(){root.refresh()} }
 RowLayout {
  Layout.fillWidth: true
  Label {text: backend.currentId===1?qsTr("All workspaces · newest events first"):qsTr("This workspace and its children");Layout.fillWidth:true;color:"#aeb9c9"}
  Button {text:qsTr("Attach session");onClicked:attachment.open()}
  Button {text:qsTr("Git snapshot");enabled:!backend.taskBusy;onClicked:backend.captureGit()}
  Button {text:qsTr("Git settings");onClicked:gitDialog.open()}
 }
 RowLayout {
  Layout.fillWidth:true
  Button {text:qsTr("Create focused chat");onClicked:focus.open()}
  Label {text:backend.gitState.managed?backend.gitState.branch:qsTr("Project folder linked · branch not managed");elide:Text.ElideMiddle;Layout.fillWidth:true;color:"#9198a6"}
  BusyIndicator {running:backend.taskBusy;visible:running;Layout.preferredWidth:28;Layout.preferredHeight:28}
 }
 ListView {
  id:events;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:8;model:root.rows
  ScrollBar.vertical:ScrollBar {}
  delegate:Rectangle {
   required property var modelData
   width:events.width;height:eventColumn.implicitHeight+24;color:"#1a2029";radius:theme.radius
   ColumnLayout {id:eventColumn;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12;spacing:7
    Label {text:"#"+modelData.id+"  ·  "+modelData.created+"  ·  "+modelData.name;Layout.fillWidth:true;elide:Text.ElideMiddle;font.pixelSize:11;color:theme.accent}
    Label {text:modelData.kind+"  —  "+modelData.summary;Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:14}
   }
   MouseArea {anchors.fill:parent;onClicked:{root.detailId=modelData.id;root.detail=backend.readEvent(modelData.id,0);eventDialog.open()}}
  }
  footer:Button {text:qsTr("Load older events");visible:root.rows.length>0;onClicked:{var before=root.rows[root.rows.length-1].id;root.rows=root.rows.concat(backend.eventPage(backend.currentId,before))}}
 }
 Label {visible:root.rows.length===0;text:qsTr("New chats, attachments, commands, and Git activity appear here.");color:"#9198a6";wrapMode:Text.Wrap;Layout.fillWidth:true}
 Dialog {
  id:eventDialog;parent:Overlay.overlay;anchors.centerIn:parent;width:Math.min(760,parent.width-40);height:Math.min(630,parent.height-40);modal:true;title:root.detail.summary||qsTr("Event detail");standardButtons:Dialog.Close
  ColumnLayout {anchors.fill:parent
   Label {text:(root.detail.source||"")+"\n"+(root.detail.created||"");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#9198a6"}
   ScrollView {Layout.fillWidth:true;Layout.fillHeight:true;TextArea {text:root.detail.content||root.detail.error||"";readOnly:true;selectByMouse:true;wrapMode:TextArea.Wrap;font.family:"monospace";font.pixelSize:12}}
   Button {text:qsTr("Read next page");enabled:root.detail.nextOffset!==undefined&&root.detail.nextOffset>=0;onClicked:root.detail=backend.readEvent(root.detailId,root.detail.nextOffset)}
  }
 }
 Dialog {
  id:attachment;parent:Overlay.overlay;anchors.centerIn:parent;width:540;modal:true;title:qsTr("Attach session or command log");standardButtons:Dialog.Open|Dialog.Cancel
  onAccepted:backend.attachSession(attachmentPath.text)
  ColumnLayout {anchors.fill:parent
   TextField {id:attachmentPath;Layout.fillWidth:true;placeholderText:qsTr("Absolute path or ~/path/to/session.jsonl")}
   Label {text:qsTr("Imports a private text snapshot up to 256 KiB. Common credentials are redacted. Session data stays outside Git. Model context tools can read this attachment when answering your requests.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#9198a6"}
  }
 }
 Dialog {
  id:focus;parent:Overlay.overlay;anchors.centerIn:parent;width:540;modal:true;title:qsTr("Focus this conversation");standardButtons:Dialog.Save|Dialog.Cancel
  onAccepted:{var id=backend.focusWorkspace(backend.currentId,focusName.text,handoff.text);if(id>0)backend.openChat(id)}
  ColumnLayout {anchors.fill:parent
   TextField {id:focusName;Layout.fillWidth:true;placeholderText:qsTr("Area of focus")}
   TextArea {id:handoff;Layout.fillWidth:true;Layout.preferredHeight:150;placeholderText:qsTr("Short goal, decisions, and next step");wrapMode:TextArea.Wrap}
   Label {text:qsTr("Creates a child chat with a compact handoff and references to this history. Use Git settings there to create its own isolated branch.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#9198a6"}
  }
 }
 Dialog {
  id:gitDialog;parent:Overlay.overlay;anchors.centerIn:parent;width:600;modal:true;title:qsTr("Background Git");standardButtons:Dialog.Save|Dialog.Cancel
  onOpened:{filePaths.text=backend.gitState.paths||"";remoteName.text=backend.gitState.remote||"";automatic.checked=backend.gitState.automatic||false;pushEnabled.checked=backend.gitState.push||false}
  onAccepted:backend.configureGit(filePaths.text,remoteName.text,automatic.checked,pushEnabled.checked)
  ColumnLayout {anchors.fill:parent;spacing:12
   Label {text:backend.gitState.managed?backend.gitState.folder:qsTr("Create a worktree from the linked repository's committed HEAD. Your original folder and its uncommitted changes stay in place.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
   Button {text:qsTr("Create isolated branch and worktree");enabled:!backend.gitState.managed&&!backend.taskBusy;onClicked:backend.forkGit()}
   TextArea {id:filePaths;Layout.fillWidth:true;Layout.preferredHeight:130;placeholderText:qsTr("Exact files to checkpoint, one relative path per line");wrapMode:TextArea.Wrap;enabled:backend.gitState.managed}
   TextField {id:remoteName;Layout.fillWidth:true;placeholderText:qsTr("Existing remote name, e.g. origin");enabled:backend.gitState.managed}
   CheckBox {id:automatic;text:qsTr("Checkpoint selected files every minute");enabled:backend.gitState.managed}
   CheckBox {id:pushEnabled;text:qsTr("Push managed branch to this remote after checkpoint");enabled:backend.gitState.managed}
   Button {text:qsTr("Checkpoint now using saved settings");enabled:backend.gitState.managed&&!backend.taskBusy;onClicked:backend.checkpointGit()}
   Label {text:qsTr("Failures pause automatic Git. It never force-pushes, merges, rebases, or deletes branches. Raw sessions remain local. Only select files you intend to commit and share.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#9198a6";font.pixelSize:12}
  }
 }
}
