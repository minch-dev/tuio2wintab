// This file is required by the index.html file and will
// be executed in the renderer process for that window.
// No Node.js APIs are available in this process because
// `nodeIntegration` is turned off. Use `preload.js` to
// selectively enable features needed in the rendering
// process.
    const electron = require('electron');
    const ipcRenderer = electron.ipcRenderer;
	var displays = {};
	var ini = {};
	var ffff = 65535;
    ipcRenderer.on('displays-changed', function (event, d) {
		displays = d;
		drawWorkspace();
    });
	function loadIniForm(){
		for (let input of document.querySelectorAll('params input')) {
			switch(input.getAttribute('type')){
				case 'text':;
					ini[input.id] = input.value;
				break;
				case 'integer':
					ini[input.id] = parseInt(input.value);
				break;
				case 'float':
					ini[input.id] = parseFloat(input.value);
				break;
				case 'checkbox':
					ini[input.id] = 1*input.checked;
				break;
				case 'file':
					//this one should be populating the corresponding text field so we dont' need to do anything here
				break;
			}
		}
		//console.log(ini);
	}
	function populateIniForm(){
		for (let input of document.querySelectorAll('params input')) {
			switch(input.getAttribute('type')){
				case 'checkbox':
					input.checked = !!ini[input.id];
				break;
				case 'file':
					//this one should be populating the corresponding text field so we dont' need to do anything here
				break;
				default:
					input.value = ini[input.id];
				break;
			}
		}		
	}
	function calculateIni(our){
		ini.wintab_x=Math.round( ((our.bounds.x - ini.total_area_x) / ini.total_area_width) *ffff );
		ini.wintab_y=Math.round( ((our.bounds.y - ini.total_area_y) / ini.total_area_height) *ffff );

		ini.wintab_w=Math.round( (our.bounds.width/ini.total_area_width) *ffff );
		ini.wintab_h=Math.round( (our.bounds.height/ini.total_area_height) *ffff );

		//the position of our monitor according to the main monitor's mouse coordinate system
		//main monitor's resolution defines pixel ratio, e.g. ffff/1024 & ffff/768
		//so for the main monitor coordinates are 0-ffff for both x & y
		//the rest are higher/lower (minus coords are fine too I suppose) of 0-ffff xy range
		//their coords are calculated according to position relative to main with ratio applied to their pixel coordinates
		ini.mouse_x=Math.round( (our.bounds.x/ini.main_width) * ffff );
		ini.mouse_y=Math.round( (our.bounds.y/ini.main_height) * ffff );

		ini.mouse_w=Math.round( (our.bounds.width/ini.main_width) * ffff );
		ini.mouse_h=Math.round( (our.bounds.height/ini.main_height) * ffff );

		//these are set in stone (0 / 0xffff), but it doesn't hurt to include those here just in case
		ini.tablet_height=ffff;
		ini.tablet_width=ffff;
		//[not implemented]
		ini.pressure_max = 1023;
		ini.pressure_min = 0;
		ini.pressure_contact = 10;
		//console.log(ini);
	}

	function selectDisplay(event){
		var index = event.target.getAttribute('index');
		for (let dn of document.querySelectorAll('display')) {
			dn.removeAttribute('selected');
		}
		event.target.setAttribute('selected','');
		ini.our_display = displays.all[index].id;
		loadIniForm();
		calculateIni(displays.all[index]);
		populateIniForm();
	}
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
			var main = '';
			if(dis.id == displays.main.id){
				main = 'main';
				ini.main_width = dis.bounds.width;
				ini.main_height = dis.bounds.height;
			}
			html += '<display onclick="selectDisplay('+d+')" index="'+d+'" '+main+' style="left:'+x+'px; top:'+y+'px; width:'+dis.bounds.width+'px; height:'+dis.bounds.height+'px;"><d style="transform:rotate('+dis.rotation+'deg);">'+(d+1)+'</d></display>';
		}
		workspace.innerHTML = html;
		workspace.style = "top:"+(-minY)+"px; left:"+(-minX)+"px;width:"+(maxX)+"px;height:"+(maxY)+"px;";
		for (let dn of document.querySelectorAll('display')) {
		  dn.onclick = selectDisplay;
		}
		//we're going to need these later anyway so why not set them now
		ini.total_area_width = maxX-minX;
		ini.total_area_height = maxY-minY;
		ini.total_area_x = minX;
		ini.total_area_y = minY;
	}