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
	$.getJSON(getApi("/api/plugins")).done(function(json) {
		// add plugins
		$.each(json, function(i, plugin) {
			var unit,
				// el = $(".template.plugin").clone().removeClass("template").appendTo(".state-plugins:first()").css({color:"red"});
				el = template(".plugin", ".state-plugins:first()").addClass("plugin-" + plugin.name);;

			if (plugin.name == "1wire") {
				unit = "°C";
				plugin.title = "1-Wire";
				el.find(".description").html("1-Wire is a device communications bus system designed by Dallas Semiconductor Corp. that provides low-speed data, signaling, and power over a single signal. (Source: <a href='https://en.wikipedia.org/wiki/1-Wire'>Wikipedia</a>)");
			}
			else if (plugin.name == "analog") {
				unit = "V";
				plugin.title = "Analog";
				el.find(".description").html("Analog plugin uses the built-in analog to digital (ADC) converter to measure analog voltages.");
			}
			else if (plugin.name == "gpio") {
				unit = "Imp";
				plugin.title = "GPIO";
				el.find(".description").html("GPIO plugin is used to register and count digital pulses.");
			}
			else if (plugin.name == "wifi") {
				unit = "dbm";
				plugin.title = "WiFi";
				el.find(".description").html("WiFi plugin measures the received signal strength indicator (RSSI) if the WiFi signal.");
			}
			el.find(".name, .title").text(plugin.title);

			// add sensors
			$.each(plugin.sensors, function(j, sensor) {
				sensor.plugin = plugin.name;
				sensor.unit = unit;
				// home screen
				template(".sensor-home", ".state-home").addClass("sensor-" + plugin.name + "-" + sensor.addr);
				// plugins screen
				var el = template(".sensor", ".state-sensors").addClass("sensor-" + plugin.name + "-" + sensor.addr);

				var sel = $(".sensor-" + plugin.name + "-" + sensor.addr);
				sel.find(".name").text(sensor.addr);
				sel.find(".value").text(sensor.value);
				sel.find(".unit").text(sensor.unit);

				if (sensor.uuid) {
					el.find(".link").html("<a href='" + getFrontend() + "?uuid[]=" + sensor.uuid + "' target='frontend'>Monitor</a>");
				}
				else {
					el.find(".sensor-connect").removeClass("hide").data(sensor);

					el.find("input").click(function() {
						// create middleware channel
						$.ajax(getMiddleware() + "/channel.json?" + $.param({
							operation: "add",
							type: "temperature",
							title: sensor.addr,
							style: "lines",
							resolution: 1
						}))
						.done(function(json) {
							if (json.entity !== undefined && json.entity.uuid !== undefined) {
								// update sensor uuid
								sensor.uuid = json.entity.uuid;
								$.ajax(getSensorApi(sensor) + "?" + $.param({
									uuid: sensor.uuid
								}))
								.done(function(json) {
									el.find(".sensor-connect").addClass("hide");
									el.find(".link").html("<a href='" + getFrontend() + "?uuid[]=" + sensor.uuid + "' target='frontend'>Monitor</a>");
									notify("success", "Sensor associated", "The sensor " + sensor.addr + " is now successfully connected. Sensor data will be directly logged to the middleware.")
								})
								.fail(function() {
									notify("error", "Sensor not connected", "Failed to update sensor " + sensor.addr + " with middleware identifier.")
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

function updateSensors(to) {
	$.ajax(getApi("/api/plugins"), {
		timeout: to
	})
	.done(function(json) {
		// plugins
		$.each(json, function(i, plugin) {
			// sensors
			$.each(plugin.sensors, function(j, sensor) {
				$(".sensor-" + plugin.name + "-" + sensor.addr + " .value").text(sensor.value);
			});
		});
	})
	.fail(function() {
		notify("warning", "No connection", "Could not update sensors from VZero.");
	});
}

function getApi(api) {
	return api;
	return "http://192.168.0.30" + api;
}

function getSensorApi(data) {
	return getApi("/api/" + data.plugin + "/" + data.addr);
}

function getMiddleware() {
	var mw = $(".middleware").val();
	if (mw == "[middleware]") {
		mw = "http://localhost:8888/vz/htdocs/middleware.php";
		$(".middleware").val(mw);
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

function heartBeat(to) {
	$.ajax(getApi('/api/status'), {
		timeout: to
	})
	.done(function(json) {
		// [0: "DEFAULT", 1: "WDT", 2: "EXCEPTION", 3: "SOFT_WDT", 4: "SOFT_RESTART", 5: "DEEP_SLEEP_AWAKE", 6: "EXT_SYS_RST"]
		if (json["resetcode"] == 1 || json["resetcode"] == 3) {
			notify("error", "Unexpected restart", "The VZero has experienced an unexpected restart, caused by the built-in watch dog timer.");
		}
		else if (json["resetcode"] == 2) {
			notify("error", "Unexpected restart", "The VZero has experienced an unexpected restart, caused by an exception.");
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
		if (json["minheap"]) {
			heap += " (min " + Math.round(json["minheap"] / 1024) + 'kB' + ")";
		}
		$(".heap").text(heap);

		var flash = json["flash"];
		if (flash > 1024) {
			flash = Math.round(flash / 1024) + 'kB';
		}
		$(".flash").text(flash);

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
	})
	.fail(function() {
		notify("warning", "No connection", "Could not update status from VZero.");
	});
}

function notify(type, title, message) {
	var hash = "hash-" + hashCode(message),
		el = $("." + hash);
	if (el.length > 0) {
		el.data({created: Date.now()});
		return;
	}
	// unhide message area
	$(".footer-container").removeClass("hide");
	// add message
	var el = template("." + type, ".messages").addClass(hash).data({created: Date.now()});
	el.find(".title").text(title);
	el.find(".message").text(message);
}

function menu(sel) {
	$(".menu a").removeClass("em");
	$(".menu a[href=#" + sel + "]").addClass("em");

	$("body > .column.row:not(.state-always)").addClass("hide");
	$(".column.row.state-" + sel + ", .column.row.state-menu").removeClass("hide");
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
		menu("home");
		$(".menu a").click(function() {
			menu($(this).attr("href").slice(1));
		});

		initializePlugins();

		sparkline = new Sparkline($(".spark")[0], {
			height: 30,
			width: $(".spark").width(),
			lineColor: "#666",
			endColor: "#0292C0",
			min: 0
		});

		heartBeat(5000);
		window.setInterval(function() {
			heartBeat(5000);
		}, 5000);
		window.setInterval(function() {
			updateSensors(10000);
		}, 10000);

		window.setInterval(function() {
			var now = Date.now();
			$(".messages .row").each(function(i, el) {
				if (now - $(el).data().created > 10000) {
					$(el).fadeOut("slow", function() {
						$(el).remove();
						if ($(".messages .row").length == 0) {
							$(".messages").addClass("hide");
						}
					});
				}
			});
		}, 2000);
	}
});
