import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
 id:root
 property var service:backend.agents
 property var job:service.selected
 property var identity:job.target&&job.target.identity?job.target.identity:({route:qsTr("Direct agent run"),mark:(job.id||"").slice(0,6),color:theme.accent})
 function openPrompt(){followup.text="";prompt.open();followup.forceActiveFocus()}
 function submit(){if(service.start(followup.text,job.target||{},job.provider||"codex",job.id||"").length)prompt.close()}
 Label {text:qsTr("Headless agents");font.pixelSize:22;Layout.fillWidth:true}
 Label {text:qsTr("Select a wheel target, press 0 and describe the task. Running jobs appear at the top of the desktop.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
 RowLayout {
  Layout.fillWidth:true;Layout.fillHeight:true;spacing:18
  ListView {
   Layout.preferredWidth:260;Layout.fillHeight:true;clip:true;model:root.service.jobs
   delegate:ItemDelegate {
    required property var modelData
    width:ListView.view.width;height:78;highlighted:root.job.id===modelData.id
    contentItem:Column {
     Label {text:modelData.title;textFormat:Text.PlainText;font.bold:true;width:parent.width;elide:Text.ElideRight}
     Label {text:qsTr("%1 · %2").arg(modelData.provider).arg(modelData.state);textFormat:Text.PlainText;color:modelData.state==="attention"?"#f1b593":theme.accent}
     Label {text:modelData.identity&&modelData.identity.mark?qsTr("%1 · %2").arg(modelData.identity.mark).arg(modelData.identity.route):modelData.id.slice(0,6);textFormat:Text.PlainText;font.pixelSize:10;color:modelData.identity&&modelData.identity.color?modelData.identity.color:"#aeb9c9";width:parent.width;elide:Text.ElideMiddle}
    }
    onClicked:root.service.select(modelData.id)
   }
  }
  ColumnLayout {
   Layout.fillWidth:true;Layout.fillHeight:true
   Label {text:root.job.title||qsTr("No run selected");textFormat:Text.PlainText;font.pixelSize:20;Layout.fillWidth:true}
   Label {visible:!!root.job.target&&!!root.job.target.cloudModel;text:root.job.target&&root.job.target.cloudModel?root.job.target.modelRole+" · "+root.job.target.cloudModel+" · Ollama cloud":"";textFormat:Text.PlainText;Layout.fillWidth:true}
   Label {text:qsTr("%1 · %2").arg(root.identity.mark).arg(root.identity.route);textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true;color:root.identity.color}
   Label {visible:!!root.job.receipt&&!!root.job.receipt.usage;Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;color:"#aeb9c9";text:root.job.receipt&&root.job.receipt.usage?qsTr("Receipt: %1 · usage %2 · functionality unverified").arg(root.job.receipt.outcome).arg(root.job.receipt.usage.status):""}
   Label {visible:!!root.job.loop;Layout.fillWidth:true;wrapMode:Text.Wrap;color:"#aeb9c9";text:root.job.loop&&root.job.loop.usageKnown?qsTr("Reported tokens: %1 input / %2 output · follow-up %3").arg(root.job.loop.inputTokens).arg(root.job.loop.outputTokens).arg(root.job.loop.depth):qsTr("Token usage not reported · the loop will not assume zero")}
   Label {text:root.job.detail||"";textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true;color:theme.accent}
   Label {text:root.job.prompt||"";textFormat:Text.PlainText;wrapMode:Text.Wrap;maximumLineCount:4;elide:Text.ElideRight;Layout.fillWidth:true}
   ScrollView {Layout.fillWidth:true;Layout.fillHeight:true
    TextArea {text:root.job.output||qsTr("Output will appear here.");readOnly:true;selectByMouse:true;wrapMode:TextEdit.Wrap;textFormat:TextEdit.PlainText}
   }
   RowLayout {
    Button {text:qsTr("Output terminal");enabled:!!root.job.id;onClicked:root.service.terminal(root.job.id)}
    Button {text:qsTr("Prompt again");enabled:!!root.job.id;onClicked:root.openPrompt()}
    Button {text:qsTr("Stop");enabled:root.job.state==="running";onClicked:root.service.stop(root.job.id)}
    Button {text:qsTr("Dismiss badge");enabled:!!root.job.id&&root.job.state!=="running";onClicked:root.service.dismiss(root.job.id)}
   }
  }
 }
 Label {text:root.service.problem;visible:text.length>0;wrapMode:Text.Wrap;textFormat:Text.PlainText;Layout.fillWidth:true;color:"#f1b593"}
 Dialog {
  id:prompt;title:qsTr("Prompt this agent");modal:true;anchors.centerIn:parent;width:Math.min(500,root.width-24)
  contentItem:ColumnLayout {
   Label {text:qsTr("A new run will use the previous output as context.");wrapMode:Text.Wrap;Layout.fillWidth:true}
   ScrollView {Layout.fillWidth:true;Layout.preferredHeight:120
    TextArea {id:followup;objectName:"agentFollowup";wrapMode:TextEdit.Wrap;placeholderText:qsTr("What next?");Keys.onPressed:function(event){if(event.key===Qt.Key_Enter||event.key===Qt.Key_Return){event.accepted=true;if(!event.isAutoRepeat)root.submit()}}}
   }
   Label {text:root.service.problem;visible:text.length>0;textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true}
   RowLayout {Button {text:qsTr("Enter  Run");onClicked:root.submit()}Button {text:qsTr("Cancel");onClicked:prompt.close()}}
  }
 }
}
