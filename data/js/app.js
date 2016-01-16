var status = -1,
	apicall = 0,
	ota = 0,
	sparkdata = [],
	sparkline;

hashCode = function(str) {
  var hash = 0, i, chr, len;
  if (str.length === 0) return hash;
  for (i = 0, len = str.length; i < len; i++) {
    chr   = str.charCodeAt(i);
    hash  = ((hash << 5) - hash) + chr;
    hash |= 0; // Convert to 32bit integer
  }
  return hash;
};

function template(tpl, target) {
	return $(".template" + tpl).clone().removeClass("template").appendTo(target);
}

function initializePlugins() {
	// json = [{"name":"1wire","settings":{"interval":30},"sensors":[{"addr":"28-17B650040000","uuid":"34b635a0-bbb3-11e5-9267-35c4f7cff544","value":21.50}]}];
	$.getJSON("http://192.168.0.30/api/plugins").done(function(json) {
		// add plugins
		$.each(json, function(i, plugin) {
			var unit,
				// el = $(".template.plugin").clone().removeClass("template").appendTo(".state-plugins:first()").css({color:"red"});
				el = template(".plugin", ".state-plugins:first()");


			if (plugin.name == "1wire") {
				unit = "°C";
				plugin.title = "1-Wire";

				el.find(".name, .title").text(plugin.title);
				el.find(".description").html("1-Wire is a device communications bus system designed by Dallas Semiconductor Corp. that provides low-speed data, signaling, and power over a single signal. (Source: <a href='https://en.wikipedia.org/wiki/1-Wire'>Wikipedia</a>)");
			}
			else if (plugin.name == "analog") {
				unit = "V";
				plugin.title = "Analog";

				el.find(".name, .title").text(plugin.title);
				el.find(".description").html("Analog plugin uses the built-in analog to digital (ADC) converter to measure analog voltages.");
			}

			// add sensors
			$.each(plugin.sensors, function(j, sensor) {
				sensor.plugin = plugin.name;

				// var el = $(".template.sensor").clone().removeClass("template").addClass("sensor-" + plugin.name + "-" + sensor.addr).appendTo(".state-sensors");
				var el = template(".sensor", ".state-sensors").addClass("sensor-" + plugin.name + "-" + sensor.addr);

				el.find(".name").text(sensor.addr);
				el.find(".value").text(sensor.value + unit);

				if (sensor.uuid) {
					el.find(".link").html("<a href='" + getFrontend() + "?uuid[]=" + sensor.uuid + "' target='frontend'>Monitor</a>");
				}
				else {
					// var el = $(".template.sensor-connect").removeClass("template").removeClass("hide").data(sensor).appendTo(".state-sensors");
					var el = template(".sensor-connect", ".state-sensors").removeClass("hide").data(sensor);

					el.find("input").click(function() {
						$.ajax(getMiddleware() + "/channel.json?" + $.param({
							operation: "add",
							type: "temperature",
							title: sensor.addr,
							style: "lines",
							resolution: 1
						})).done(function(json) {
							console.log(json);

							if (json.entity !== undefined && json.entity.uuid !== undefined) {
								$.ajax(getSensorApi(sensor) + "?" + $.param({
									uuid: json.entity.uuid
								})).done(function(json) {
									notify("success", "Sensor associated", "The sensor " + sensor.addr + " is now successfully connected. Sensor data will be directly logged to the middleware.")
								}).fail(function() {
									notify("error", "Sensor not connected", "Could not connect sensor " + sensor.addr + " to the middleware.")
								});
							}
						}).fail(function() {
							notify("error", "Sensor not connected", "Could not connect sensor " + sensor.addr + " to the middleware.")
						});
					});
				}
			});
		});
	});
}

function getMiddleware() {
	var mw = $(".middleware").val();
	if (mw == "[middleware]") {
		mw = "http://localhost:8888/vz/htdocs/middleware.php";
	}
	if (mw.length && mw[mw.length-1] == '/') {
		mw = mw.substring(0, mw.length-1);
	}
	return mw;
}

function getFrontend() {
	var mw = getMiddleware();
	var i = mw.indexOf("middleware");
	if (i >= 0) {
		mw = mw.substring(0, i-1);
	}
	if (mw.length && mw[mw.length-1] == '/') {
		mw = mw.substring(0, mw.length-1);
	}
	mw += "/frontend";
	return mw;
}

function getSensorApi(data) {
	return "http://192.168.0.30/api/" + data.plugin + "/" + data.addr;
	return "/api/" + data.plugin + "/" + data.addr;
}

function notify(type, title, message) {
	var hash = hashCode(message);
	if ($(".hash-" + hash).length > 0) {
		return hash;
	}

	// var el = $(".template." + type).clone().removeClass("template").addClass("hash-" + hash).appendTo(".messages");
	var el = template(type, ".messages").addClass("hash-" + hash);
	el.find(".title").text(title);
	el.find(".message").text(message);
	return(hash);
}

function updateStatus(json) {
	// [0: "DEFAULT", 1: "WDT", 2: "EXCEPTION", 3: "SOFT_WDT", 4: "SOFT_RESTART", 5: "DEEP_SLEEP_AWAKE", 6: "EXT_SYS_RST"]
	console.log(json);
	if (json["resetcode"] >= 1 && json["resetcode"] <= 3) {
		notify("warning", "Unexpected restart", "The VZero has experienced an unexpected restart, typically caused by an exception or the built-in watch dog timer. Please contact support if this happens regularly.", true);
	}

	if (json["ota"] == 0 && ota > 0) {
		notify("success", "OTA completed", "Over-the-air update successfully completed. VZero can be safely restarted or disconnected from power source.");
	}
	else if (json["ota"] == 1) {
		notify("warning", "OTA in progress", "Over-the-air update in progress. Make sure VZero remains connected to a power source while update is in progress.");
	}
	else if (json["ota"] == 2) {
		notify("error", "OTA failed", "Over-the-air update failed. Consider re-trying OTA update before restarting VZero or disconnecting from power source.");
	}
	ota = json["ota"];

	var heap = json["heap"];
	if (heap > 1024) {
		heap = Math.round(heap / 1024) + 'kB';
	}
	$(".heap").text(heap);

	var date = new Date(json["uptime"]);
	var hours = parseInt(date / 3.6e6) % 24;
	$(".uptime").text(
		(hours < 10 ? "0" + hours : hours) +":"+ ("0"+date.getMinutes()).slice(-2) +":"+ ("0"+date.getSeconds()).slice(-2)
	);

	sparkdata.push(json["heap"]);
	if (sparkdata.length > 50) {
		sparkdata.shift();
	}
	sparkline.draw(sparkdata);
}

function heartBeat() {
	// avoid overloading esp
	if (apicall > 0) {
		return;
	}
	apicall++;
	$.getJSON('/api/status').done(function(json) {
		apicall--;
		updateStatus(json);
	})
	.fail(function() {
		apicall--;
		updateStatus({
			"uptime": 1.12346e6,
			"heap": 35e3,
			"resetcode": 0
		});
	});
}

$(document).ready(function() {
	// status = 1;

	// initial setup - wifi
	if ($(".wifimode").text() == "Access Point" || status == 0) {
		$(".column.row:not(.state-always), .menu-container").addClass("hide");
		$(".state-initial-wifi").removeClass("hide");
	}
	// initial setup - middleware
	else if ($(".middleware").val() == "" || status == 1) {
		$(".column.row:not(.state-always), .menu-container").addClass("hide");
		$(".state-initial-middleware").removeClass("hide");
	}
	else {
		// menu
		$(".menu a").click(function() {
			$(".menu a").removeClass("em");
			$(this).addClass("em");
			$("body > .column.row:not(.state-always):not(.template)").addClass("hide");
			$(".column.row.state-" + $(this).attr("state") + ", .column.row.state-menu").removeClass("hide");
		});

		initializePlugins();

		sparkline = new Sparkline($(".spark")[0], {
			height: 30,
			width: $(".spark").width(),
			lineColor: "#666",
			endColor: "#0292C0",
			min: 0
		});

		heartBeat();
		window.setInterval(heartBeat, 10000);
	}
});
