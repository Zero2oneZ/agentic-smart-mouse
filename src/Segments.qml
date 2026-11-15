import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
 id: root
 property var rows: []
 property var selected: ({})
 property var detail: ({})
 property string partId: ""
 property string problem: ""
 spacing: 10
 function refresh() {rows=backend.segmentPage(stateFilter.currentText===qsTr("All")?"":stateFilter.currentText,search.text,0);if(selected.id){var current=backend.segmentAction("read",{id:selected.id});if(!current.error)selected=current;else selected=({})}}
 function act(action,args){var result=backend.segmentAction(action,args||{});problem=result.error||"";if(result.id&&result.state)selected=result;return result}
 Component.onCompleted: refresh()
 Connections {target:backend;function onActivityChanged(){root.refresh()}}
 RowLayout {
  Layout.fillWidth:true
  TextField {id:search;Layout.fillWidth:true;placeholderText:qsTr("Find a segment by description, summary, kind, or ID");onTextChanged:root.refresh()}
  ComboBox {id:stateFilter;model:[qsTr("All"),"collecting","ready","processing","complete","failed"];onCurrentIndexChanged:root.refresh()}
  Button {text:qsTr("New segment");onClicked:createDialog.open()}
 }
 RowLayout {
  Layout.fillWidth:true
  Button {text:qsTr("Segment this chat");onClicked:root.act("from_chat",{})}
  Button {text:qsTr("Process next ready");onClicked:root.act("process",{})}
  CheckBox {text:qsTr("Auto-process with local extractive worker");checked:backend.segmentAuto;onToggled:backend.segmentAuto=checked}
 }
 Label {visible:root.problem.length>0;text:root.problem;Layout.fillWidth:true;wrapMode:Text.Wrap;color:"#efbd91"}
 SplitView {
  Layout.fillWidth:true;Layout.fillHeight:true
  ListView {
   id:cards;SplitView.preferredWidth:330;SplitView.minimumWidth:200;clip:true;spacing:8;model:root.rows
   ScrollBar.vertical:ScrollBar {}
   delegate:Rectangle {
    required property var modelData
    width:cards.width;height:cardColumn.implicitHeight+24;radius:theme.radius;color:root.selected.id===modelData.id?"#293241":"#1a2029"
    ColumnLayout {id:cardColumn;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12;spacing:6
     Label {text:modelData.kind+" · "+modelData.state;color:theme.accent;Layout.fillWidth:true;elide:Text.ElideRight;font.pixelSize:11}
     Label {text:modelData.title;font.pixelSize:15;wrapMode:Text.Wrap;Layout.fillWidth:true}
     Label {text:modelData.summary||modelData.description;color:"#aeb9c9";wrapMode:Text.Wrap;maximumLineCount:3;elide:Text.ElideRight;Layout.fillWidth:true;font.pixelSize:12}
     Label {text:modelData.id;color:"#778397";elide:Text.ElideMiddle;Layout.fillWidth:true;font.pixelSize:10}
    }
    MouseArea {anchors.fill:parent;onClicked:root.act("read",{id:modelData.id})}
   }
   footer:Button {text:qsTr("Load older segments");visible:root.rows.length>0;onClicked:{var last=root.rows[root.rows.length-1];root.rows=root.rows.concat(backend.segmentPage(stateFilter.currentIndex===0?"":stateFilter.currentText,search.text,last.seq))}}
  }
  ScrollView {
   id:segmentDetail;contentWidth:availableWidth
   SplitView.fillWidth:true;SplitView.minimumWidth:250
   ColumnLayout {
    width:segmentDetail.availableWidth;spacing:12
    Label {text:root.selected.title||qsTr("Choose a segment");font.pixelSize:20;wrapMode:Text.Wrap;Layout.fillWidth:true}
    TextArea {text:root.selected.id||"";readOnly:true;selectByMouse:true;wrapMode:TextArea.Wrap;Layout.fillWidth:true;font.pixelSize:11}
    Label {text:root.selected.description||qsTr("Preserve session evidence, queue it for later processing, and combine summaries by reference.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#b7c0ce"}
    RowLayout {
     Button {text:qsTr("Add artifact");enabled:root.selected.state==="collecting";onClicked:artifactDialog.open()}
     Button {text:qsTr("Make ready");enabled:root.selected.state==="collecting";onClicked:root.act("ready",{id:root.selected.id})}
     Button {text:qsTr("Retry");enabled:root.selected.state==="failed";onClicked:root.act("retry",{id:root.selected.id})}
    }
    RowLayout {
     Button {text:qsTr("Link another segment");enabled:root.selected.state==="collecting";onClicked:linkDialog.open()}
     Button {text:qsTr("Compose bundle");onClicked:bundleDialog.open()}
    }
    Label {text:qsTr("SUMMARY");color:theme.accent;font.pixelSize:11;visible:!!root.selected.summary}
    TextArea {text:root.selected.summary||"";visible:text.length>0;readOnly:true;selectByMouse:true;wrapMode:TextArea.Wrap;Layout.fillWidth:true}
    Label {text:root.selected.lastError||"";visible:text.length>0;wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#efbd91"}
    Label {text:qsTr("ORIGINALS AND DERIVED RESULTS");color:theme.accent;font.pixelSize:11}
    Repeater {model:root.selected.artifacts||[]
     delegate:Button {
      required property var modelData
      text:modelData.kind+" · "+modelData.id.slice(0,20)+"…";Layout.fillWidth:true;Layout.minimumWidth:0
      onClicked:{root.partId=modelData.id;root.detail=backend.segmentAction("part",{id:modelData.id,offset:0});partDialog.open()}
     }
    }
    Label {text:qsTr("LINKED SEGMENTS");color:theme.accent;font.pixelSize:11;visible:(root.selected.links||[]).length>0}
    Repeater {model:root.selected.links||[]
     delegate:Button {required property var modelData;text:modelData.relation+" · "+(modelData.from===root.selected.id?modelData.to:modelData.from);Layout.fillWidth:true;onClicked:root.act("read",{id:modelData.from===root.selected.id?modelData.to:modelData.from})}
    }
   }
  }
 }
 Dialog {
  id:createDialog;parent:Overlay.overlay;anchors.centerIn:parent;width:540;modal:true;title:qsTr("New session segment");standardButtons:Dialog.Save|Dialog.Cancel
  onAccepted:root.act("create",{kind:segmentKind.currentText,title:segmentTitle.text,description:description.text})
  ColumnLayout {anchors.fill:parent
   ComboBox {id:segmentKind;model:["web_search","code","analysis","chat","experiment"];Layout.fillWidth:true}
   TextField {id:segmentTitle;placeholderText:qsTr("Specific task or research question");Layout.fillWidth:true}
   TextArea {id:description;placeholderText:qsTr("Short description for later discovery");wrapMode:TextArea.Wrap;Layout.fillWidth:true;Layout.preferredHeight:130}
  }
 }
 Dialog {
  id:artifactDialog;parent:Overlay.overlay;anchors.centerIn:parent;width:620;height:500;modal:true;title:qsTr("Preserve an artifact");standardButtons:Dialog.Save|Dialog.Cancel
  onAccepted:root.act("add",{id:root.selected.id,kind:artifactKind.currentText,content:artifactText.text,source:source.text})
  ColumnLayout {anchors.fill:parent
   ComboBox {id:artifactKind;model:["input","output","script","history","source","notes"];Layout.fillWidth:true}
   TextField {id:source;placeholderText:qsTr("Source URL, path, or command reference");Layout.fillWidth:true}
   ScrollView {Layout.fillWidth:true;Layout.fillHeight:true;TextArea {id:artifactText;placeholderText:qsTr("Original text, script, I/O, search history, or result");wrapMode:TextArea.Wrap;selectByMouse:true}}
   Label {text:qsTr("Stored locally with common credentials redacted. Scripts are preserved as text; this action does not execute them.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#9198a6"}
  }
 }
 Dialog {
  id:linkDialog;parent:Overlay.overlay;anchors.centerIn:parent;width:540;modal:true;title:qsTr("Link reusable context");standardButtons:Dialog.Save|Dialog.Cancel
  onAccepted:root.act("link",{id:root.selected.id,target:targetId.text.trim(),relation:relation.currentText})
  ColumnLayout {anchors.fill:parent;TextField {id:targetId;placeholderText:qsTr("Target seg_… ID");Layout.fillWidth:true}ComboBox {id:relation;model:["uses","derived_from","continues"];Layout.fillWidth:true}}
 }
 Dialog {
  id:bundleDialog;parent:Overlay.overlay;anchors.centerIn:parent;width:540;modal:true;title:qsTr("Compose a bundle of segments");standardButtons:Dialog.Save|Dialog.Cancel
  onOpened:componentIds.text=root.selected.id||""
  onAccepted:root.act("compose",{title:bundleTitle.text,ids:componentIds.text.split(/[,\s]+/).filter(function(id){return id.length>0})})
  ColumnLayout {anchors.fill:parent;TextField {id:bundleTitle;placeholderText:qsTr("Bundle title");Layout.fillWidth:true}TextArea {id:componentIds;placeholderText:qsTr("Segment IDs, separated by commas or new lines");wrapMode:TextArea.Wrap;Layout.fillWidth:true;Layout.preferredHeight:150}}
 }
 Dialog {
  id:partDialog;parent:Overlay.overlay;anchors.centerIn:parent;width:Math.min(760,parent.width-40);height:Math.min(630,parent.height-40);modal:true;title:qsTr("Preserved artifact");standardButtons:Dialog.Close
  ColumnLayout {anchors.fill:parent
   TextArea {text:root.partId;readOnly:true;selectByMouse:true;Layout.fillWidth:true;font.pixelSize:11}
   ScrollView {Layout.fillWidth:true;Layout.fillHeight:true;TextArea {text:root.detail.content||root.detail.error||"";readOnly:true;selectByMouse:true;wrapMode:TextArea.Wrap;font.family:"monospace";font.pixelSize:12}}
   Button {text:qsTr("Read next page");enabled:root.detail.nextOffset!==undefined&&root.detail.nextOffset>=0;onClicked:root.detail=backend.segmentAction("part",{id:root.partId,offset:root.detail.nextOffset})}
  }
 }
}
