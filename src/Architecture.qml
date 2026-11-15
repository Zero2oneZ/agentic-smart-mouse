import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
 id: root
 objectName: "architectureView"
 property var registry: backend.architecture
 property var preview: ({})
 property string problem: ""
 clip: true
 function arguments() {
  var caps=[]
  if(music.checked)caps.push("music")
  if(sounds.checked)caps.push("sounds")
  if(video.checked)caps.push("video")
  if(pictures.checked)caps.push("pictures")
  if(covers.checked)caps.push("album-covers")
  for(var item of extraCapabilities.text.split(",")){item=item.trim();if(item.length)caps.push(item)}
  return {capabilities:caps,artist:artist.text,album:album.text,genre:genre.text}
 }
 function invalidate(){preview=({});problem=""}
 function act(action,args){var result=backend.architectureAction(action,args);problem=result.error||"";return result}
 Connections {target:backend;function onChanged(){root.invalidate()}}
 ColumnLayout {
  width:root.availableWidth
  spacing:14
  Label {text:qsTr("Shared architecture");font.pixelSize:23}
  Label {text:qsTr("Build from a shared template. Keep application source in the development Git tree and personal files in a separate belongings tree. Attach existing storage across projects without copying it.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
  Label {text:qsTr("Storage foundation · container execution is not configured");color:"#f1d99b";wrapMode:Text.Wrap;Layout.fillWidth:true}
  GroupBox {
  title:qsTr("Runtime and data locations");Layout.fillWidth:true
  ColumnLayout {anchors.fill:parent
   property var report:{var registry=root.registry;return backend.runtimeReport()}
   Label {text:parent.report.databaseOk?qsTr("Private database check passed"):qsTr("Private database needs attention");Layout.fillWidth:true;wrapMode:Text.Wrap}
   Repeater {model:parent.report.storage||[]
    delegate:Label {required property var modelData;Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;text:modelData.name+"\n"+modelData.path;color:"#aeb9c9"}
   }
   Label {text:qsTr("Agent terminals show output. Interactive terminal attachment is not connected.");Layout.fillWidth:true;wrapMode:Text.Wrap;color:"#9198a6"}
  }
 }
 GroupBox {
   title:root.registry.initialized?qsTr("Architecture roots"):qsTr("Initialize architecture from master chat")
   Layout.fillWidth:true
   ColumnLayout {
    anchors.fill:parent
    Label {visible:backend.currentId!==1&&!root.registry.initialized;text:qsTr("Select Master chat to initialize the shared development environment.");wrapMode:Text.Wrap;Layout.fillWidth:true}
    Label {text:qsTr("Tool-owned development monorepo");visible:backend.currentId===1}
    TextField {id:toolRoot;visible:backend.currentId===1;Layout.fillWidth:true;readOnly:!!root.registry.initialized;text:root.registry.toolRoot||root.registry.defaultToolRoot||"";placeholderText:qsTr("Absolute development root")}
    Label {text:qsTr("User-owned belongings (outside Git)");visible:backend.currentId===1}
    TextField {id:runtimeRoot;visible:backend.currentId===1;Layout.fillWidth:true;readOnly:!!root.registry.initialized;text:root.registry.runtimeRoot||root.registry.defaultRuntimeRoot||"";placeholderText:qsTr("Absolute belongings root")}
    Button {visible:backend.currentId===1&&!root.registry.initialized;text:qsTr("Create roots and initialize Git");onClicked:root.act("init",{toolRoot:toolRoot.text,runtimeRoot:runtimeRoot.text})}
    Label {visible:!!root.registry.initialized;text:qsTr("Template: media-v1. Choose a focused workspace with no linked folder to create its project. Future projects reuse the same library paths.");wrapMode:Text.Wrap;Layout.fillWidth:true}
   }
  }
  GroupBox {
   visible:!!root.registry.initialized&&backend.currentId===1
   title:qsTr("Extend the architecture")
   Layout.fillWidth:true
   ColumnLayout {
    anchors.fill:parent
    Label {text:qsTr("Register a new library before projects use it. This adds a reusable definition to the Git tree; the user directory is created only when a project requests it.");wrapMode:Text.Wrap;Layout.fillWidth:true}
    TextField {id:capabilityId;Layout.fillWidth:true;placeholderText:qsTr("Capability ID, e.g. documents");maximumLength:40}
    TextField {id:capabilityPath;Layout.fillWidth:true;placeholderText:qsTr("Relative location, e.g. libraries/documents");maximumLength:170}
    TextField {id:capabilityDescription;Layout.fillWidth:true;placeholderText:qsTr("Reusable purpose (no personal information)");maximumLength:240}
    Button {text:qsTr("Register capability");onClicked:root.act("define",{id:capabilityId.text,path:capabilityPath.text,description:capabilityDescription.text})}
   }
  }
  Label {visible:!!root.registry.initialized;text:qsTr("Registered libraries: ")+((root.registry.template||{}).capabilities||[]).map(function(item){return item.id}).join(", ");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
  GroupBox {
   visible:!!root.registry.initialized&&backend.currentId!==1
   title:qsTr("Storage for %1").arg(backend.title)
   Layout.fillWidth:true
   ColumnLayout {
    anchors.fill:parent
    Flow {
     Layout.fillWidth:true;spacing:8
     CheckBox {id:music;text:qsTr("Music");onToggled:root.invalidate()}
     CheckBox {id:sounds;text:qsTr("Sounds");onToggled:root.invalidate()}
     CheckBox {id:video;text:qsTr("Video");onToggled:root.invalidate()}
     CheckBox {id:pictures;text:qsTr("Pictures");onToggled:root.invalidate()}
     CheckBox {id:covers;text:qsTr("Album covers");onToggled:root.invalidate()}
    }
    TextField {id:extraCapabilities;Layout.fillWidth:true;placeholderText:qsTr("Additional registered capability IDs, separated by commas");onTextChanged:root.invalidate()}
    Label {text:qsTr("Optional music labels. Genres reference canonical artists or albums; they do not duplicate songs. Album covers share the same artist and album identities.");wrapMode:Text.Wrap;Layout.fillWidth:true;color:"#aeb9c9"}
    TextField {id:artist;Layout.fillWidth:true;placeholderText:qsTr("Artist / band (optional)");maximumLength:120;onTextChanged:root.invalidate()}
    TextField {id:album;Layout.fillWidth:true;placeholderText:qsTr("Album (requires artist)");maximumLength:120;onTextChanged:root.invalidate()}
    TextField {id:genre;Layout.fillWidth:true;placeholderText:qsTr("Genre (optional)");maximumLength:120;onTextChanged:root.invalidate()}
    RowLayout {
     Button {text:qsTr("Preview directories");onClicked:root.preview=root.act("plan",root.arguments())}
     Button {text:qsTr("Create missing and attach existing");enabled:!!root.preview.ok;onClicked:{var result=root.act("ensure",root.arguments());if(result.ok)root.preview=result}}
    }
   }
  }
  Label {visible:root.problem.length>0;text:root.problem;color:"#f1d99b";wrapMode:Text.Wrap;Layout.fillWidth:true}
  Repeater {
   model:root.preview.directories||[]
   delegate:Label {
    required property var modelData
    Layout.fillWidth:true;wrapMode:Text.Wrap;font.family:"monospace";font.pixelSize:12
    text:(modelData.existing?qsTr("Reuse"):qsTr("Create"))+" · "+modelData.owner+" · "+modelData.path
   }
  }
  Label {text:qsTr("Attached user storage");font.pixelSize:18}
  Label {visible:!(root.registry.attachments||[]).length;text:qsTr("No storage attached in this workspace yet.");color:"#aeb9c9"}
  Repeater {
   model:root.registry.attachments||[]
   delegate:Label {required property var modelData;Layout.fillWidth:true;wrapMode:Text.Wrap;text:"#"+modelData.category+" · "+modelData.capability+"\n"+modelData.path;font.pixelSize:12}
  }
  Label {text:qsTr("Genre associations");visible:(root.registry.genres||[]).length>0;font.pixelSize:18}
  Repeater {
   model:root.registry.genres||[]
   delegate:Label {required property var modelData;Layout.fillWidth:true;wrapMode:Text.Wrap;text:modelData.genre+" → "+modelData.entity;font.pixelSize:12}
  }
 }
}
