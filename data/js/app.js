var status = -1,
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

function ajax(url, settings) {
	var log = (settings.data) ? url + "<br/>" + JSON.stringify(settings.data) : url;
	var hash = notify("info", "API Log", log, true);
	return $.ajax(url, settings).done(function(json) {
		$("." + hash + " .message").html($("." + hash + " .message").html() + "<br/>" + JSON.stringify(json));
	});
}

function template(tpl, target) {
	return $(".template" + tpl).clone().removeClass("template").appendTo(target);
}

function initializePlugins() {
	$.getJSON(getApi("/api/plugins")).done(function(json) {
		// add plugins
		$.each(json, function(i, plugin) {
			var unit,
				el = template(".plugin", ".state-plugins:first()").addClass("plugin-" + plugin.name);

			$(".state-plugins .not-configured").remove();

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

			if (json && json.length) {
				$(".state-plugins .not-configured").remove();
			}

			// add sensors
			$.each(plugin.sensors, function(j, sensor) {
				$(".state-sensors .not-configured").remove();

				sensor.plugin = plugin.name;
				sensor.unit = unit;
				// home screen
				template(".sensor-home", ".state-home").addClass("sensor-" + sensor.plugin + "-" + sensor.addr);
				// plugins screen
				var el = template(".sensor", ".state-sensors").addClass("sensor-" + sensor.plugin + "-" + sensor.addr);
				updateSensorUI(sensor);
			});
		});
	});
}

function updateSensorUI(sensor) {
	var el = $(".sensor-" + sensor.plugin + "-" + sensor.addr);
	el.find(".name").text(sensor.addr);
	el.find(".value").text(sensor.value);
	el.find(".unit").text(sensor.unit);

	el.find(".sensor-monitor").unbind().click(function() {
		// "<a href='" + getFrontend() + "?uuid[]=" + sensor.uuid + "' target='frontend'>Monitor</a>");
		window.open(getFrontend() + "?uuid[]=" + sensor.uuid, "frontend");
	});

	if (sensor.uuid) {
		el.find(".sensor-connect").addClass("hide");
		el.find(".sensor-monitor, .sensor-disconnect, .sensor-delete").removeClass("hide");
		el.find(".sensor-disconnect").unbind().click(function() {
			disconnectSensor(sensor);
		});
		el.find(".sensor-delete").unbind().click(function() {
			disconnectSensor(sensor, true);
		});
	}
	else {
		el.find(".sensor-monitor, .sensor-disconnect, .sensor-delete").addClass("hide");
		el.find(".sensor-connect").removeClass("hide").unbind().click(function() {
			connectSensor(sensor);
		});
	}
}

function connectSensor(sensor) {
	// 1) check if sensor already exists at middleware
	$.ajax(getMiddleware() + "/iot/" + sensor.hash + ".json")
	.then(function(json) {
		if (json.entities === undefined) {
			// wrong mw version
			notify("warning", "Middleware failed", "Could not perform middleware operation. Check middleware version (needs d63104b).")
		}
		else if (json.entities.length === 1) {
			// exactly one match
			return $.Deferred().resolveWith(this, [json.entities[0].uuid]);
		}
		// 2) if not create channel
		var data = {
			operation: "add",
			type: getSensorType(sensor.plugin),
			title: sensor.addr,
			style: "lines",
			resolution: 1
		};
		if (json.entities !== undefined) {
			// assume recent middleware
			$.extend(data, {
				owner: sensor.hash
			});
		}
		return $.ajax(getMiddleware() + "/channel.json?" + $.param(data))
		.then(function(json) {
			if (json.entity !== undefined && json.entity.uuid !== undefined) {
				return $.Deferred().resolveWith(this, [json.entity.uuid]);
			}
			notify("error", "Sensor not connected", "Unknown middleware error.")
			return $.Deferred.reject();
		}, function() {
			notify("error", "Sensor not connected", "Could not connect sensor " + sensor.addr + " to the middleware.")
			return $.Deferred.reject();
		});
	}, function() {
		notify("error", "Middleware error", "Could not connect to middleware.");
	})
	.done(function(uuid) {
		// 3) update sensor config with uuid
		sensor.uuid = uuid;
		$.ajax(getSensorApi(sensor) + "?" + $.param({
			uuid: sensor.uuid
		}))
		.done(function(json) {
			notify("success", "Sensor associated", "The sensor " + sensor.addr + " is now successfully connected. Sensor data will be directly logged to the middleware.");
			updateSensorUI(sensor);
		})
		.fail(function() {
			notify("error", "Sensor not connected", "Failed to update sensor " + sensor.addr + " with middleware identifier.");
		});
	});
}

function disconnectSensor(sensor, fullDelete) {
	var deferred = (fullDelete) ?
		$.ajax(getMiddleware() + "/channel/" + sensor.uuid + ".json?" + $.param({
			operation: "delete"
		})) :
		$.Deferred().resolveWith(this, [{}]);

	deferred.done(function(json) {
		if (json.exception !== undefined) {
			notify("error", "Middleware error", "Could not delete sensor " + sensor.addr + " from middleware. Sensor will be disconnected instead.");
		}
		$.ajax(getSensorApi(sensor) + "?" + $.param({
			uuid: ""
		}))
		.done(function(json) {
			delete sensor.uuid;
			notify("success", "Sensor disconnected", "The sensor " + sensor.addr + " has been successfully disconnected. Sensor data will no longer be logged to the middleware.");
			updateSensorUI(sensor);
		})
		.fail(function() {
			notify("error", "Sensor not disconnected", "Failed to delete middleware identifier from sensor " + sensor.addr + ".");
		});
	})
	.fail(function() {
		notify("error", "Sensor not disconnected", "Could not disconnect sensor " + sensor.addr + " from the middleware.")
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

function getSensorType(plugin) {
	switch (plugin) {
		case "1wire":
			return "temperature";
		case "analog":
			return "voltage";
		case "wifi":
			return "rssi";
		default:
			return "powersensor";
	}
}

function getApi(api) {
	// return api;
	// return "http://cpuidle.ddns.net:3000" + api;
	return "http://192.168.0.30" + api;
}

function getSensorApi(data) {
	return getApi("/api/" + data.plugin + "/" + data.addr);
}

function getMiddleware() {
	var mw = $(".middleware").val();
	if (mw == "") {
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

function heartBeat(to, initial) {
	return $.ajax(getApi('/api/status' + (initial ? "?initial=1" : "")), {
		timeout: to || 5000
	})
	.done(function(json) {
		// [0: "DEFAULT", 1: "WDT", 2: "EXCEPTION", 3: "SOFT_WDT", 4: "SOFT_RESTART", 5: "DEEP_SLEEP_AWAKE", 6: "EXT_SYS_RST"]
		if (json.resetcode == 1 || json.resetcode == 3) {
			notify("error", "Unexpected restart", "The VZero has experienced an unexpected restart, caused by the built-in watch dog timer.");
		}
		else if (json.resetcode == 2) {
			notify("error", "Unexpected restart", "The VZero has experienced an unexpected restart, caused by an exception.");
		}

		var heap = json.heap;
		if (heap < 16384) {
			notify("warning", "Low memory", "Available memory has reached a critical limit. VZero might become unstable.")
		}
		if (heap > 1024) {
			heap = Math.round(heap / 1024) + 'kB';
		}
		if (json.minheap) {
			heap += " (min " + Math.round(json.minheap / 1024) + 'kB' + ")";
		}
		$(".heap").text(heap);

		var flash = json.flash;
		if (flash > 1024) {
			flash = Math.round(flash / 1024) + 'kB';
		}
		$(".flash").text(flash);

		var date = new Date(json.uptime);
		var hours = parseInt(date / 3.6e6) % 24;
		$(".uptime").text(
			(hours < 10 ? "0" + hours : hours) +":"+ ("0"+date.getMinutes()).slice(-2) +":"+ ("0"+date.getSeconds()).slice(-2)
		);

		if (sparkline) {
			sparkdata.push(json.heap);
			if (sparkdata.length > 50) {
				sparkdata.shift();
			}
			sparkline.draw(sparkdata);
		}
		return $.Deferred().resolve(json);
	})
	.fail(function() {
		notify("warning", "No connection", "Could not update status from VZero.");
	});
}

function notify(type, title, message, force) {
	var hash = "hash" + hashCode(message);
	if (force) hash += Math.floor(Math.random() * Number.MAX_SAFE_INTEGER);
	var el = $("." + hash);
	if (el.length > 0) {
		el.data({created: Date.now()});
		return hash;
	}
	// unhide message area
	$(".footer-container").removeClass("hide");
	// add message
	el = template("." + type, ".messages").addClass(hash).data({created: Date.now()});
	el.find(".title").text(title);
	el.find(".message").text(message);
	return hash;
}

function menu(sel) {
	$(".menu a").removeClass("em");
	$(".menu a[href=#" + sel + "]").addClass("em");

	$("body > .column.row:not(.state-always)").addClass("hide");
	$(".column.row.state-" + sel + ", .column.row.state-menu").removeClass("hide");
}

function connectDevice() {
	heartBeat(5000, true).done(function(json) {
		$('.loader').remove();
		$('.content > *').unwrap();

		$('title').text($('title').text() + " (" + json.serial + ")");
		$('.ip').text(json.ip);
		$('.wifimode').text(json.wifimode);
		$('.serial').text(json.serial);
		$('.build').text(json.build);
		$('.ssid').val(json.ssid);
		$('.pass').val(json.pass);
		$('.middleware').val(json.middleware);

		// initial setup - wifi
		if ($(".wifimode").text() == "Access Point" || status == 0) {
			$(".column.row:not(.state-always), .menu-container").addClass("hide");
			$(".state-initial-wifi").removeClass("hide");
		}
		// initial setup - middleware
		else if ($(".middleware").val() == "" || status == 1) {
			$(".middleware").val("http://demo.volkszaehler.org/middleware.php");
			$(".column.row:not(.state-always), .menu-container").addClass("hide");
			$(".state-initial-middleware").removeClass("hide");
		}
		else {
			menu(window.location.hash.substr(1) || "home");
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

			heartBeat(10000);
			window.setInterval(function() {
				heartBeat(10000);
			}, 10000);
			window.setInterval(function() {
				updateSensors(30000);
			}, 30000);

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
	}).fail(connectDevice);
}

$(document).ready(function() {
	// status = 1;
	$('.loader .subheader').text("connecting...");
	connectDevice();
});
