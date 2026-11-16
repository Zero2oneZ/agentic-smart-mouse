import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
 id:root;objectName:"toolsView"
 property string groupFilter:""
 function newTool(group){editor.editing=null;prepare();nameField.text="";purposeField.text="";groupField.text=group||"General";editor.open()}
 function prepare(){editor.requestKey="request_"+Date.now()+"_"+Math.floor(Math.random()*1000000);editor.problem="";editor.ownerCategory=backend.currentId;editor.ownerLabel=backend.title}
 function editTool(row){editor.editing=row;prepare();nameField.text=row.name;purposeField.text=row.purpose||"";groupField.text=row.menu_category;editor.open()}
 spacing:14
 RowLayout {Layout.fillWidth:true
  ColumnLayout {Layout.fillWidth:true
   Label {text:qsTr("Your tools");font.pixelSize:26;font.bold:true}
   Label {text:qsTr("Categorized tools with their own development history.");color:"#aeb9c9";wrapMode:Text.Wrap;Layout.fillWidth:true}
  }
  Button {text:qsTr("Refresh");enabled:!backend.toolRegistryBusy;onClicked:backend.refreshToolRegistry()}
  Button {text:qsTr("+ New tool");enabled:backend.toolRegistryReady;onClicked:root.newTool(root.groupFilter)}
 }
 Label {visible:backend.toolRegistryBusy;text:qsTr("Checking tool registry…");color:"#aeb9c9"}
 RowLayout {visible:backend.toolRegistryProblem.length>0;Layout.fillWidth:true
  Label {text:backend.toolRegistryProblem;textFormat:Text.PlainText;wrapMode:Text.Wrap;color:"#f1c493";Layout.fillWidth:true}
  Button {text:qsTr("Retry saved requests");enabled:!backend.toolRegistryBusy;onClicked:backend.retryToolRegistry()}
 }
 TextField {Layout.fillWidth:true;placeholderText:qsTr("Filter by category…");text:root.groupFilter;onTextEdited:root.groupFilter=text}
 Label {visible:backend.registeredTools.length===0;text:qsTr("Create a tool to give it a place in Hotbox and a development chat. Saving a draft does not run an agent.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
 ListView {id:toolList;objectName:"registeredToolList";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:10
  model:backend.registeredTools.filter(function(row){return !root.groupFilter.length||(row.menu_category||"").toLowerCase().indexOf(root.groupFilter.toLowerCase())>=0})
  ScrollBar.vertical:ScrollBar {}
  delegate:Rectangle {required property var modelData
   width:ListView.view.width;height:card.implicitHeight+28;radius:8;color:"#1a2029";border.color:"#354254"
   ColumnLayout {id:card;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:14;spacing:8
    Label {text:modelData.menu_category||"";textFormat:Text.PlainText;color:theme.accent;font.pixelSize:11}
    Label {text:modelData.name||"";textFormat:Text.PlainText;font.pixelSize:20;font.bold:true;wrapMode:Text.Wrap;Layout.fillWidth:true}
    Label {text:modelData.purpose||"";textFormat:Text.PlainText;color:"#bdc6d4";wrapMode:Text.Wrap;Layout.fillWidth:true}
    Label {text:modelData.sync_state==="pending"?qsTr("Saving registration…"):modelData.sync_state==="error"?(modelData.sync_error||qsTr("Registration needs attention")):modelData.verified?qsTr("Draft · ready to edit"):qsTr("Saved locally · checking registry");textFormat:Text.PlainText;color:"#aeb9c9";wrapMode:Text.Wrap;Layout.fillWidth:true}
    RowLayout {
     Button {objectName:"editToolContext";text:qsTr("Edit");enabled:modelData.verified;onClicked:backend.openToolContext(modelData.tool_id)}
     Button {text:qsTr("Name and category");enabled:modelData.verified&&modelData.sync_state!=="pending";onClicked:root.editTool(modelData)}
     Button {text:qsTr("Run");enabled:false;ToolTip.visible:hovered;ToolTip.text:modelData.run_reason||""}
    }
    Label {text:modelData.run_reason||"";font.pixelSize:11;color:"#8997aa";wrapMode:Text.Wrap;Layout.fillWidth:true}
   }
  }
 }
 Dialog {id:editor;objectName:"toolDraftEditor";parent:Overlay.overlay;anchors.centerIn:parent;width:Math.min(540,parent.width-40);modal:true
  property var editing:null
  property string requestKey:""
  property string problem:""
  property int ownerCategory:1
  property string ownerLabel:""
  title:editing?qsTr("Tool name and category"):qsTr("Create a tool");standardButtons:Dialog.Cancel
  ColumnLayout {anchors.fill:parent;spacing:12
   Label {text:editor.editing?qsTr("This keeps the same tool and development context."):qsTr("A development chat will be linked to %1. Nothing runs when you save.").arg(editor.ownerLabel);textFormat:Text.PlainText;wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
   TextField {id:nameField;objectName:"toolDraftName";Layout.fillWidth:true;placeholderText:qsTr("Tool name")}
   TextField {id:groupField;objectName:"toolDraftCategory";Layout.fillWidth:true;placeholderText:qsTr("Hotbox category, for example Writing")}
   ScrollView {Layout.fillWidth:true;Layout.preferredHeight:140;TextArea {id:purposeField;objectName:"toolDraftPurpose";placeholderText:qsTr("What should this tool do?");wrapMode:TextEdit.Wrap}}
   Label {visible:editor.problem.length>0;text:editor.problem;textFormat:Text.PlainText;color:"#f1c493";wrapMode:Text.Wrap;Layout.fillWidth:true}
   Button {objectName:"saveToolDraft";text:qsTr("Save tool");enabled:backend.toolRegistryReady&&nameField.text.trim().length>0&&groupField.text.trim().length>0
    onClicked:{var input={request_id:editor.requestKey,name:nameField.text,purpose:purposeField.text,menu_category:groupField.text};if(editor.editing){input.tool_id=editor.editing.tool_id;input.expected_revision=editor.editing.revision}else input.category=editor.ownerCategory;var result=editor.editing?backend.updateToolDraft(input):backend.registerToolDraft(input);if(result.ok)editor.close();else editor.problem=result.error||qsTr("Could not save this tool.")}
   }
  }
 }
}
