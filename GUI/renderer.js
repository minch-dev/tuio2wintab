// This file is required by the index.html file and will
// be executed in the renderer process for that window.
// No Node.js APIs are available in this process because
// `nodeIntegration` is turned off. Use `preload.js` to
// selectively enable features needed in the rendering
// process.
    const electron = require('electron');
    const ipcRenderer = electron.ipcRenderer;
	var displays = {};
    ipcRenderer.on('displays-changed', function (event, d) {
		displays = d;
		drawWorkspace();
    });
	function drawWorkspace(){
		var workspace = document.getElementsByTagName('workspace')[0];
		var html = '';
		var minX = 0;
		var minY = 0;
		var maxX = 0;
		var maxY = 0;
		for(d=0;d<displays.all.length;d++){
			var dis = displays.all[d];
			var x=dis.bounds.x;
			var y=dis.bounds.y;
			var w=dis.bounds.width;
			var h=dis.bounds.height;
			var x2=x+w;
			var y2=y+h;
			if(x<minX){minX=x;}
			if(y<minY){minY=y;}
			if(x2>maxX){maxX=x2;}
			if(y2>maxY){maxY=y2;}
			html += '<display index="'+d+'" class="'+(dis.id == displays.main.id ? 'main' : '')+'" style="left:'+x+'px; top:'+y+'px; width:'+dis.bounds.width+'px; height:'+dis.bounds.height+'px;"><d style="transform:rotate('+dis.rotation+'deg);">'+(d+1)+'</d></display>';
		}
		workspace.innerHTML = html;
		workspace.style = "top:"+(-minY)+"px; left:"+(-minX)+"px;width:"+(maxX)+"px;height:"+(maxY)+"px;";
	}