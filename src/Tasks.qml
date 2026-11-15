import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
 id: root
 objectName: "tasksView"
 property var service: backend.tasks
 property var job: service ? service.selected : ({})
 property bool composing: false
 property bool details: false
 clip: true
 function newTask() {composing=true; titleField.forceActiveFocus()}
 function statusName(code) {
  return ({queued:qsTr("Ready to start"),working:qsTr("Working"),paused:qsTr("Paused"),complete:qsTr("Finished"),cancelled:qsTr("Stopped"),failed:qsTr("Could not finish")})[code]||""
 }
 function phase(step) {return [qsTr("Preparing your workspace"),qsTr("Organizing your notes"),qsTr("Checking the result"),qsTr("Saving your brief"),qsTr("Your brief is ready")][Math.min(step||0,4)]}
 ColumnLayout {
  width:root.availableWidth;spacing:18
  RowLayout {
   Layout.fillWidth:true
   ColumnLayout {
    Layout.fillWidth:true
    Label {text:qsTr("A little help, a finished result.");font.pixelSize:24;wrapMode:Text.Wrap;Layout.fillWidth:true}
    Label {text:qsTr("Turn your notes into a tidy brief. Everything stays on this computer.");color:"#aeb9c9";wrapMode:Text.Wrap;Layout.fillWidth:true}
   }
   Button {objectName:"newTaskButton";text:qsTr("New task");onClicked:root.newTask()}
  }
  Frame {
   Layout.fillWidth:true
   ColumnLayout {
    anchors.fill:parent;spacing:10
    Label {text:qsTr("Ready when you are");font.pixelSize:16}
    Label {text:!root.service?qsTr("Task setup is unavailable."):root.service.problem||(!root.service.available?qsTr("Background tasks need an update."):qsTr("Local task service ready. No AI connection or extra downloads needed."));textFormat:Text.PlainText;color:root.service&&root.service.available&&!root.service.problem?"#a9cfb0":"#efbd91";wrapMode:Text.Wrap;Layout.fillWidth:true}
    RowLayout {
     Button {text:qsTr("Check setup");enabled:!!root.service&&!root.service.busy;onClicked:root.service.retry()}
     Button {text:qsTr("Try sample notes");enabled:!!root.service&&root.service.available&&!root.service.busy;onClicked:{titleField.text=qsTr("Weekend project");notesField.text=qsTr("Choose a quiet corner for a reading space.\nMeasure the wall before buying shelves.\nReuse the small lamp from the bedroom.\nMeasure the wall before buying shelves.\nKeep room for a comfortable chair.");root.newTask()}}
    }
   }
  }
  Frame {
   visible:root.composing;Layout.fillWidth:true
   ColumnLayout {
    anchors.fill:parent;spacing:12
    Label {text:qsTr("Organize notes");font.pixelSize:20}
    Label {text:qsTr("One note per line. We'll keep the order, remove repeated lines, check the brief, and save a new file.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
    Label {text:qsTr("What is this for?")}
    TextField {id:titleField;objectName:"taskTitle";Layout.fillWidth:true;maximumLength:100;placeholderText:qsTr("For example, Weekend project");Accessible.name:qsTr("Task title")}
    Label {text:qsTr("Your notes")}
    ScrollView {Layout.fillWidth:true;Layout.preferredHeight:160
     TextArea {id:notesField;objectName:"taskNotes";placeholderText:qsTr("Paste your notes here…");wrapMode:TextArea.Wrap;selectByMouse:true;Accessible.name:qsTr("Your notes")}
    }
    Label {text:qsTr("Access: only the text you paste here. Results go to a private task folder. Existing files stay as they are.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
    Label {visible:notesField.text.length>65000;text:qsTr("These notes are too long. Try a smaller set.");color:"#efbd91";wrapMode:Text.Wrap;Layout.fillWidth:true}
    RowLayout {
     Button {objectName:"startTaskButton";text:qsTr("Start task");enabled:!!root.service&&root.service.available&&!root.service.busy&&titleField.text.trim().length>0&&notesField.text.trim().length>0&&notesField.text.length<=65000;onClicked:{root.service.start(titleField.text,notesField.text,backend.currentId);root.composing=false}}
     Button {text:qsTr("Close");onClicked:root.composing=false}
    }
   }
  }
  Label {text:qsTr("Your work");font.pixelSize:20}
  Label {visible:!root.service||!root.service.jobs.length;text:qsTr("Nothing running yet. Try the sample or start with your own notes.");wrapMode:Text.Wrap;color:"#aeb9c9";Layout.fillWidth:true}
  Repeater {
   model:root.service?root.service.jobs:[]
   delegate:Button {
    required property var modelData
    Layout.fillWidth:true;Layout.preferredHeight:62
    enabled:!!root.service&&!root.service.busy
    highlighted:root.job.id===modelData.id
    text:modelData.title+"  ·  "+root.statusName(modelData.status)
    contentItem:Label {text:parent.text;textFormat:Text.PlainText;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter;leftPadding:12;rightPadding:12}
    Accessible.name:text
    onClicked:{root.details=false;root.service.select(modelData.id)}
   }
  }
  Frame {
   visible:!!root.job.id;Layout.fillWidth:true
   ColumnLayout {
    anchors.fill:parent;spacing:12
    Label {text:root.job.title||"";textFormat:Text.PlainText;wrapMode:Text.Wrap;font.pixelSize:21;Layout.fillWidth:true}
    Label {text:root.statusName(root.job.status);font.pixelSize:14;color:root.job.status==="failed"?"#efbd91":theme.accent}
    Label {text:root.job.status==="complete"?qsTr("Saved and checked. You can open your brief below."):root.job.status==="paused"?qsTr("Your place is saved. Continue whenever you're ready."):root.job.status==="cancelled"?qsTr("This task has stopped. Your original notes are kept."):root.job.status==="failed"?qsTr("Your work is kept. Check Details for the reason, or start a new task."):root.phase(root.job.step);wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
    RowLayout {
     Layout.fillWidth:true
     Repeater {model:[qsTr("Prepare"),qsTr("Organize"),qsTr("Check"),qsTr("Save")]
      delegate:Label {required property string modelData;required property int index;text:(index<(root.job.step||0)?"✓  ":"○  ")+modelData;color:index<(root.job.step||0)?"#a9cfb0":"#9198a6";Layout.fillWidth:true}
     }
    }
    RowLayout {
     Button {visible:root.job.status==="queued"||root.job.status==="working";text:qsTr("Pause");enabled:!!root.service&&!root.service.busy;onClicked:root.service.control("pause")}
     Button {visible:root.job.status==="paused";text:qsTr("Continue");enabled:!!root.service&&!root.service.busy;onClicked:root.service.control("resume")}
     Button {visible:root.job.status==="queued"||root.job.status==="working"||root.job.status==="paused";text:qsTr("Stop");enabled:!!root.service&&!root.service.busy;onClicked:root.service.control("cancel")}
     Button {visible:root.job.status==="complete";text:qsTr("Open brief");onClicked:root.service.openResult(false)}
     Button {visible:root.job.status==="complete";text:qsTr("Show in folder");onClicked:root.service.openResult(true)}
     Button {text:root.details?qsTr("Hide details"):qsTr("Details");onClicked:root.details=!root.details}
    }
    ScrollView {visible:!!root.job.preview;Layout.fillWidth:true;Layout.preferredHeight:240
     TextArea {text:root.job.preview||"";textFormat:TextEdit.PlainText;readOnly:true;selectByMouse:true;wrapMode:TextArea.Wrap;Accessible.name:qsTr("Saved brief preview")}
    }
    TextArea {visible:root.details;text:[root.job.id||"",root.job.resource||"",root.job.result_path||"",root.job.error||""].filter(function(s){return s.length>0}).join("\n");readOnly:true;selectByMouse:true;textFormat:TextEdit.PlainText;wrapMode:TextArea.Wrap;Layout.fillWidth:true;font.pixelSize:11}
   }
  }
  Label {text:qsTr("Work continues while Hotbox stays in the background. After a restart, unfinished local tasks pick up from their saved step.");wrapMode:Text.Wrap;color:"#9198a6";Layout.fillWidth:true;font.pixelSize:12}
 }
}
