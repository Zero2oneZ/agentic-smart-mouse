import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
 id:root
 objectName:"buildLedgerView"
 property var roads:[]
 property var road:({})
 property string step:""
 property string problem:""
 property var document:({})
 property bool documentLatest:{if(!document.version)return true;for(var item of road.documents||[])if(item.document===document.document&&item.number>document.number)return false;return true}
 function call(operation,args){var request=Object.assign({schema_version:1},args||{});if(!["list","read","history","document.read","checkpoint.read"].includes(operation)){request.request_id=Date.now()+"-"+Math.random();if(road.id&&operation!=="create")request.expected_revision=road.revision}var result=backend.buildAction(operation,request);problem=result.error||"";return result}
 function refresh(){roads=call("list",{}).roadmaps||[];if(road.id){var current=call("read",{roadmap:road.id});if(current.ok)road=current}}
 function choose(id){road=call("read",{roadmap:id});step="";document={}}
 function mutate(operation,args){var result=call(operation,Object.assign({roadmap:road.id},args||{}));if(result.ok){if(operation==="fork")choose(result.roadmap);refresh()}return result}
 Component.onCompleted:refresh()
 Connections {target:backend;function onActivityChanged(){root.refresh()}}
 Label {text:qsTr("Build roadmap");font.pixelSize:23;Layout.fillWidth:true}
 Label {text:qsTr("Milestones, steps, evidence and document history stay linked to Master chat. Planning state is separate from verified execution.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
 RowLayout {
  TextField {id:newTitle;objectName:"buildRoadTitle";placeholderText:qsTr("New roadmap title");Layout.fillWidth:true;maximumLength:160}
  Button {text:qsTr("Create");onClicked:{var result=root.call("create",{title:newTitle.text});if(result.ok){root.choose(result.roadmap);root.refresh();newTitle.clear()}}}
  Button {text:qsTr("Refresh");onClicked:root.refresh()}
 }
 RowLayout {
  Layout.fillWidth:true;Layout.fillHeight:true
  ListView {Layout.preferredWidth:230;Layout.fillHeight:true;model:root.roads;clip:true
   delegate:ItemDelegate {required property var modelData;width:ListView.view.width;text:modelData.title;highlighted:root.road.id===modelData.id;contentItem:Label {text:modelData.title;textFormat:Text.PlainText;elide:Text.ElideRight}onClicked:root.choose(modelData.id)}
  }
  ColumnLayout {
   Layout.fillWidth:true;Layout.fillHeight:true
   Label {text:root.road.title||qsTr("Select or create a roadmap");textFormat:Text.PlainText;font.pixelSize:18;Layout.fillWidth:true;elide:Text.ElideRight}
   Label {text:root.road.id?qsTr("Revision %1 · Master chat linked · %2 evidence links").arg(root.road.revision).arg((root.road.links||[]).length):"";Layout.fillWidth:true;color:theme.accent}
   RowLayout {
    Button {text:qsTr("Add item");enabled:!!root.road.id;onClicked:nodeDialog.open()}
    Button {text:qsTr("Checkpoint");enabled:!!root.road.id;onClicked:root.mutate("checkpoint",{})}
    Button {text:qsTr("Fork");enabled:!!root.road.id;onClicked:{forkTitle.text=root.road.title+qsTr(" branch");forkDialog.open()}}
   }
   ListView {Layout.fillWidth:true;Layout.fillHeight:true;clip:true;model:root.road.nodes||[]
    delegate:ItemDelegate {required property var modelData;width:ListView.view.width;highlighted:root.step===modelData.id
     contentItem:Label {text:(modelData.parent?"    > ":"> ")+modelData.kind+" · "+modelData.title+" · "+modelData.state;textFormat:Text.PlainText;elide:Text.ElideRight}
     onClicked:root.step=modelData.id
    }
   }
   RowLayout {
    Button {text:qsTr("Record issue");enabled:!!root.road.id;onClicked:{nodeKind.currentIndex=4;nodeDialog.open()}}
    Button {text:qsTr("Mark blocked");enabled:!!root.step;onClicked:root.mutate("node.update",{node:root.step,state:"blocked"})}
    Button {text:qsTr("Add document");enabled:!!root.road.id;onClicked:{root.document={};docTitle.clear();docBody.clear();documentDialog.open()}}
    Button {text:qsTr("Prompt step");enabled:!!root.step;onClicked:runDialog.open()}
   }
   ComboBox {id:versions;Layout.fillWidth:true;model:root.road.documents||[];textRole:"title";contentItem:Label {text:versions.displayText;textFormat:Text.PlainText;elide:Text.ElideRight}
    delegate:ItemDelegate {required property var modelData;required property int index;width:versions.width;highlighted:versions.highlightedIndex===index;contentItem:Label {text:modelData.title+" · v"+modelData.number;textFormat:Text.PlainText;elide:Text.ElideRight}}
    displayText:currentIndex>=0&&model.length?model[currentIndex].title+" · v"+model[currentIndex].number:qsTr("No document versions")}
   RowLayout {
    Button {text:qsTr("Open version");enabled:versions.currentIndex>=0&&(root.road.documents||[]).length>0;onClicked:{var item=versions.model[versions.currentIndex];var text="",offset=0,ok=true;for(var i=0;i<12;i++){var value=root.call("document.read",{roadmap:root.road.id,version:item.version,offset:offset});if(!value.ok){ok=false;break}text+=value.content;offset=value.nextOffset;if(offset<0)break}if(ok&&offset<0){root.document=item;docTitle.text=item.title;docBody.text=text;documentDialog.open()}}}
    Label {text:qsTr("API provides complete paged history and checkpoint forks.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#9198a6"}
   }
  }
 }
 Label {text:root.problem;visible:text.length>0;textFormat:Text.PlainText;color:"#f1b593";wrapMode:Text.Wrap;Layout.fillWidth:true}
 Dialog {id:nodeDialog;title:qsTr("Add roadmap item");modal:true;anchors.centerIn:parent;width:420;standardButtons:Dialog.Save|Dialog.Cancel
  onAccepted:{var result=root.mutate("node.add",{parent:root.step,kind:nodeKind.currentText,title:nodeTitle.text,body:nodeBody.text});if(result.ok){root.step=result.node;nodeTitle.clear();nodeBody.clear()}}
  ColumnLayout {anchors.fill:parent
   ComboBox {id:nodeKind;model:["milestone","checkpoint","step","substep","issue","test","debug","documentation"];Layout.fillWidth:true}
   TextField {id:nodeTitle;placeholderText:qsTr("Title");maximumLength:160;Layout.fillWidth:true}
   TextArea {id:nodeBody;placeholderText:qsTr("Goal, acceptance checks or issue details");Layout.fillWidth:true;Layout.preferredHeight:100;wrapMode:TextEdit.Wrap}
   Label {text:root.step?qsTr("Added under the selected item"):qsTr("Added at roadmap root");color:"#aeb9c9"}
  }
 }
 Dialog {id:forkDialog;title:qsTr("Fork roadmap");modal:true;anchors.centerIn:parent;width:420;standardButtons:Dialog.Save|Dialog.Cancel
  onAccepted:{var args={title:forkTitle.text,branch_ref:forkBranch.text};if(forkSelected.checked&&root.step)args.node=root.step;root.mutate("fork",args)}
  ColumnLayout {anchors.fill:parent
   TextField {id:forkTitle;placeholderText:qsTr("Fork title");Layout.fillWidth:true}
   CheckBox {id:forkSelected;text:qsTr("Fork selected item and its children only");enabled:!!root.step}
   TextField {id:forkBranch;placeholderText:qsTr("Declared branch reference (optional)");Layout.fillWidth:true}
   Label {text:qsTr("Preserves source history and creates linked planning IDs. This does not create a Git branch.");Layout.fillWidth:true;wrapMode:Text.Wrap}
  }
 }
 Dialog {id:documentDialog;title:qsTr("Document version");modal:true;anchors.centerIn:parent;width:Math.min(650,root.width);height:450
  contentItem:ColumnLayout {
   TextField {id:docTitle;readOnly:!!root.document.version;placeholderText:qsTr("Document title");Layout.fillWidth:true}
   ScrollView {Layout.fillWidth:true;Layout.fillHeight:true;TextArea {id:docBody;textFormat:TextEdit.PlainText;readOnly:!!root.document.version&&!root.documentLatest;wrapMode:TextEdit.Wrap;selectByMouse:true}}
   Label {visible:!!root.document.version&&!root.documentLatest;text:qsTr("Historical versions are read-only. Fork from a checkpoint to branch their history.");wrapMode:Text.Wrap;Layout.fillWidth:true}
   Button {text:root.document.version?qsTr("Save new version"):qsTr("Save new document");enabled:root.documentLatest;onClicked:{var args={node:root.step,title:docTitle.text,content:docBody.text};if(root.document.version){args.document=root.document.document;args.parent_version=root.document.version}var result=root.mutate("document.save",args);if(result.ok)documentDialog.close()}}
   Button {text:qsTr("Close");onClicked:documentDialog.close()}
  }
 }
 Dialog {id:runDialog;title:qsTr("Prompt selected build step");modal:true;anchors.centerIn:parent;width:450
  contentItem:ColumnLayout {
   ComboBox {id:provider;model:["Codex","Claude"];Layout.fillWidth:true}
   TextArea {id:runPrompt;placeholderText:qsTr("What should this step do?");Layout.fillWidth:true;Layout.preferredHeight:110;wrapMode:TextEdit.Wrap}
   Label {text:qsTr("Runs in a private starter directory and links the output to this step. Existing project source is not automatically edited.");Layout.fillWidth:true;wrapMode:Text.Wrap}
   Button {text:qsTr("Run");onClicked:{var id=backend.agents.start(runPrompt.text,{name:root.road.title,id:root.road.workspace,action:"build",build:root.road.id,step:root.step},provider.currentIndex===0?"codex":"claude");if(id.length){runDialog.close();root.refresh()}else root.problem=backend.agents.problem}}
   Button {text:qsTr("Cancel");onClicked:runDialog.close()}
  }
 }
}
