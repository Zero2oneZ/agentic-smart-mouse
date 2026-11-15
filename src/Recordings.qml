import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ScrollView {
 id:root
 objectName:"recordingsView"
 property var recorder:backend.recorder
 property var item:recorder.current
 clip:true
 function addNote(){noteDialog.open()}
 ColumnLayout {
  width:root.availableWidth;spacing:16
  Label {text:qsTr("Teach a helper");font.pixelSize:24}
  Label {text:qsTr("Record a short workflow, review its steps, then save a private helper draft. Nothing replays until its tools, access and tests are set up.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
  Label {text:qsTr("Hotbox records only actions requested through this app and notes or links you choose to add. Browser recordings come from the companion, after you start it on one tab. Cookies stay in the browser.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
  Frame {Layout.fillWidth:true
   ColumnLayout {anchors.fill:parent;spacing:10
    TextField {id:recordingTitle;placeholderText:qsTr("What do you want to repeat?");text:qsTr("My workflow");maximumLength:100;Layout.fillWidth:true;Accessible.name:qsTr("Recording name")}
    RowLayout {
     Button {text:qsTr("Start recording");enabled:root.item.state!=="recording"&&root.item.state!=="paused";onClicked:root.recorder.start(recordingTitle.text)}
     Button {visible:root.item.state==="recording"||root.item.state==="paused";text:root.item.state==="paused"?qsTr("Continue"):qsTr("Pause");onClicked:root.recorder.togglePause()}
     Button {text:qsTr("Finish");enabled:root.item.state==="recording"||root.item.state==="paused";onClicked:root.recorder.stop()}
    }
    Label {text:root.item.state==="recording"?qsTr("● Recording Hotbox actions"):root.item.state==="paused"?qsTr("Paused — nothing is being recorded"):root.item.title||qsTr("No recording selected");textFormat:Text.PlainText;color:root.item.state==="recording"?"#f1b593":"#aeb9c9";wrapMode:Text.Wrap;Layout.fillWidth:true}
    RowLayout {
     Button {text:qsTr("Add a note");enabled:root.item.state==="recording";onClicked:root.addNote()}
     Button {text:qsTr("Add a web address");enabled:root.item.state==="recording";onClicked:urlDialog.open()}
    }
    Button {text:qsTr("Import browser recording");onClicked:importDialog.open()}
   }
  }
  Label {visible:!!root.recorder.problem;text:root.recorder.problem;textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#efbd91"}
  Repeater {model:root.item.steps||[]
   delegate:Frame {required property var modelData;required property int index;Layout.fillWidth:true
    ColumnLayout {anchors.fill:parent
     Label {text:(index+1)+". "+modelData.label;textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true}
     Label {visible:!!modelData.url;text:modelData.url;textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
     Label {visible:!!modelData.target&&modelData.kind!=="hotbox_action";text:modelData.target;textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
    }
   }
  }
  RowLayout {
   TextField {id:helperName;placeholderText:qsTr("Name your helper");maximumLength:100;Layout.fillWidth:true;Accessible.name:qsTr("Helper name")}
   Button {text:qsTr("Make helper draft");enabled:root.item.state==="stopped"&&root.item.count>0;onClicked:root.recorder.makeDraft(helperName.text)}
  }
  Frame {visible:!!root.recorder.draft.id;Layout.fillWidth:true
   ColumnLayout {anchors.fill:parent;spacing:10
    Label {text:root.recorder.draft.name||"";textFormat:Text.PlainText;font.pixelSize:20;Layout.fillWidth:true}
    Label {text:qsTr("Private draft — not enabled");color:theme.accent}
    Label {text:root.recorder.draft.review||"";wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
    Label {visible:!!root.recorder.draft.session;text:root.recorder.draft.session||"";wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
    Repeater {model:root.recorder.draft.steps||[]
     delegate:Label {required property var modelData;required property int index;text:(index+1)+". "+modelData.instruction;textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true}
    }
   }
  }
  Label {text:qsTr("Your recordings");font.pixelSize:18}
  Repeater {model:root.recorder.history
   delegate:Button {required property var modelData;text:modelData.title;Layout.fillWidth:true;onClicked:root.recorder.select(modelData.id)}
  }
 }
 Dialog {id:noteDialog;parent:Overlay.overlay;anchors.centerIn:parent;width:Math.min(560,parent.width-32);modal:true;title:qsTr("Add a note");standardButtons:Dialog.Save|Dialog.Cancel;onAccepted:if(root.recorder.addNote(noteText.text))noteText.text=""
  TextArea {id:noteText;anchors.fill:parent;wrapMode:TextArea.Wrap;placeholderText:qsTr("Describe why this step matters. Do not paste passwords or login tokens.");Accessible.name:qsTr("Recording note")}
 }
 Dialog {id:urlDialog;parent:Overlay.overlay;anchors.centerIn:parent;width:Math.min(560,parent.width-32);modal:true;title:qsTr("Add a web address");standardButtons:Dialog.Save|Dialog.Cancel;onAccepted:if(root.recorder.addUrl(urlText.text))urlText.text=""
  TextField {id:urlText;anchors.fill:parent;placeholderText:"https://example.org/page";Accessible.name:qsTr("Web address")}
 }
 FileDialog {id:importDialog;title:qsTr("Choose a browser recording");nameFilters:[qsTr("Browser recordings (*.json)")];onAccepted:root.recorder.importBrowser(selectedFile)}
}
