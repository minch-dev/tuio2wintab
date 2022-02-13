// This file is required by the index.html file and will
// be executed in the renderer process for that window.
// No Node.js APIs are available in this process because
// `nodeIntegration` is turned off. Use `preload.js` to
// selectively enable features needed in the rendering
// process.
    const electron = require('electron');
    const ipcRenderer = electron.ipcRenderer;
	const fs = require('fs');
	var displays = {};
	var ini = {Switches:{},IO:{},Metrics:{}};
	var ffff = 65535;
    ipcRenderer.on('displays-changed', function (event, d) {
		displays = d;
		drawWorkspace();
    });
	document.getElementById('exe_file').onchange = function(event){
		var path = event.target.files[0].path;
		path = path.substring(0, path.lastIndexOf("\\"));
		document.getElementById('ini_file').value = path+'\\wintab32.ini';
	}
	document.getElementById('ini_save').onclick = saveIniFile;
	
	
	function saveIniFile(){
		var ini_txt = '';
		var path = document.getElementById('ini_file').value;
		for(var section in ini){
			ini_txt += '['+section+']\r\n';
			//console.log(section);
			for (var param in ini[section]){
				var val = ini[section][param];
				if(section == 'Metrics' & param.indexOf('tuio_')==0){
					val = Math.round(val * 10000000);
				}
				ini_txt += param+'='+val+'\r\n';
				
			}
			ini_txt += '\r\n\r\n';
		}
		try { fs.writeFileSync(path, ini_txt, 'ascii'); } //need to indicate it's done later
		catch(err) { alert('Unfortunetely, we couldn\'t save this file... My condolences.'); }
		// nooooow we need to save this to a file. next time!
	}
	
	function loadIniForm(){
		for (var input of document.querySelectorAll('params input')) {
			switch(input.getAttribute('type')){
				case 'text':;
					ini[input.title][input.id] = input.value;
				break;
				case 'integer':
					ini[input.title][input.id] = parseInt(input.value || 0);
				break;
				case 'float':
					ini[input.title][input.id] = parseFloat(input.value || 0.0);
				break;
				case 'checkbox':
					ini[input.title][input.id] = 1*input.checked;
				break;
				case 'file':
					//this one should be populating the corresponding text field so we dont' need to do anything here
				break;
			}
		}
		//console.log(ini);
	}
	function populateIniForm(){
		for (var input of document.querySelectorAll('params input')) {
			switch(input.getAttribute('type')){
				case 'checkbox':
					input.checked = !!ini[input.title][input.id];
				break;
				case 'file':
					//this one should be populating the corresponding text field so we dont' need to do anything here
				break;
				default:
					input.value = ini[input.title][input.id];
				break;
			}
		}		
	}
	function calculateIni(chosen_ones){
		var x  = Infinity;
		var y  = Infinity ;
		var xx = 0;
		var yy = 0;
		var our;
		for(var c=0;c<chosen_ones.length;c++){
			our = displays.all[chosen_ones[c]];
			if(our.bounds.x < x){
				x = our.bounds.x;
			}
			if(our.bounds.y < y){
				y = our.bounds.y;
			}
			our.xx = our.bounds.x+our.bounds.width;
			if(our.xx > xx){
				xx = our.xx;
			}
			our.yy = our.bounds.y+our.bounds.height;
			if(our.yy > yy){
				yy = our.yy;
			}
		}
		var width = xx - x;
		var height = yy - y;
		//console.log(x,y,xx,yy, width, height);
		
		
		ini.Metrics.wintab_x=Math.round( ((x - ini.Metrics.total_area_x) / ini.Metrics.total_area_width) *ffff );
		ini.Metrics.wintab_y=Math.round( ((y - ini.Metrics.total_area_y) / ini.Metrics.total_area_height) *ffff );

		ini.Metrics.wintab_w=Math.round( (width/ini.Metrics.total_area_width) *ffff );
		ini.Metrics.wintab_h=Math.round( (height/ini.Metrics.total_area_height) *ffff );

		//the position of our monitor according to the main monitor's mouse coordinate system
		//main monitor's resolution defines pixel ratio, e.g. ffff/1024 & ffff/768
		//so for the main monitor coordinates are 0-ffff for both x & y
		//the rest are higher/lower (minus coords are fine too I suppose) of 0-ffff xy range
		//their coords are calculated according to position relative to main with ratio applied to their pixel coordinates
		ini.Metrics.mouse_x=Math.round( (x/ini.Metrics.main_width) * ffff );
		ini.Metrics.mouse_y=Math.round( (y/ini.Metrics.main_height) * ffff );

		ini.Metrics.mouse_w=Math.round( (width/ini.Metrics.main_width) * ffff );
		ini.Metrics.mouse_h=Math.round( (height/ini.Metrics.main_height) * ffff );

		//these are set in stone (0 / 0xffff), but it doesn't hurt to include those here just in case
		ini.Metrics.tablet_height=ffff;
		ini.Metrics.tablet_width=ffff;
		//[not implemented]
		ini.Metrics.pressure_max = 1023;
		ini.Metrics.pressure_min = 0;
		ini.Metrics.pressure_contact = 10;
		//console.log(ini);
	}

	function selectDisplay(param){
		var chosen_ones = [];
		if(typeof(param)!='object'){
			var index = parseInt(param);
			var multi = false;
		} else {
			var index = parseInt(param.target.getAttribute('index'));
			var multi = param.ctrlKey;
		}
		
		for (var dn of document.querySelectorAll('display')) {
			var i = dn.getAttribute('index');
			if(multi){
			//CTRL pressed - multi select
				if(dn.getAttribute('selected')!=null){
				//currently selected
					if(i==index){
						//that we just pressed - so reverse the state
						dn.removeAttribute('selected');
					} else {
						//selected so goes to the list
						chosen_ones.push(i);
					}
				} else {
				//currently not selected
					if(i==index){
					//that we just pressed - so reverse the state
						dn.setAttribute('selected','');
						//now selected so goes to the list
						chosen_ones.push(i);
					}
				}
			} else {
			//regular click
				if(i==index){
				//select the clicked, and it goes to the list
					dn.setAttribute('selected','');
					chosen_ones.push(i);
				} else {
				//unselect the rest
					dn.removeAttribute('selected');
				}
			}
		}
		
		if(chosen_ones.length){
			loadIniForm();
			calculateIni(chosen_ones);
			populateIniForm();
		}
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
				ini.Metrics.main_width = dis.bounds.width;
				ini.Metrics.main_height = dis.bounds.height;
			}
			html += '<display onclick="selectDisplay('+d+')" index="'+d+'" '+main+' style="left:'+x+'px; top:'+y+'px; width:'+dis.bounds.width+'px; height:'+dis.bounds.height+'px;"><d style="transform:rotate('+dis.rotation+'deg);">'+(d+1)+'</d></display>';
		}
		workspace.innerHTML = html;
		workspace.style = "top:"+(-minY)+"px; left:"+(-minX)+"px;width:"+(maxX)+"px;height:"+(maxY)+"px;";
		for (var dn of document.querySelectorAll('display')) {
		  dn.onclick = selectDisplay;
		}
		//we're going to need these later anyway so why not set them now
		ini.Metrics.total_area_width = maxX-minX;
		ini.Metrics.total_area_height = maxY-minY;
		ini.Metrics.total_area_x = minX;
		ini.Metrics.total_area_y = minY;
	}