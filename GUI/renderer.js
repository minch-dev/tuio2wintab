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
	function calculateIni(our){
		//[Switches]
		//[not implemented]
		ini.logging=0
		ini.tuio_udp=1
		ini.tuio_tcp=0

		//0 means we don't move mouse cursor while drawing, coords are passed by wintab interface and we navigate with the ending of a line drawn, best quality
		//1 means mouse is emulated while dragging and wintab is off, expect jigsaw lines (pixel rounding artifacts)
		//2..4 might be other options later
		ini.tuio_mouse=0
		//[not implemented], this prop determines if we use buttons of a modified mouse which serves as an IR stylus for the tablet, or if we use pressure levels to detect clicks
		ini.tuio_buttons=1

		//[IO]
		//[not implemented], there is a problem with "ini read" and "server start" events sync
		log_file='C:\wintab32.txt';
		ini.tuio_udp_port=3333;
		ini.tuio_udp_address='localhost';
		ini.tuio_tcp_port=3000;
		ini.tuio_tcp_address='localhost';
		
		//[Metrics]
		ini.tuio_x=0;//0325269;
		ini.tuio_y=0;
		ini.tuio_w=10000000;//9299733;
		ini.tuio_h=10000000;

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
		console.log(ini);
	}
	function selectDisplay(event){
		var index = event.target.getAttribute('index');
		for (let dn of document.querySelectorAll('display')) {
			dn.removeAttribute('selected');
		}
		event.target.setAttribute('selected','');
		ini.our_display = displays.all[index].id;
		calculateIni(displays.all[index]);
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