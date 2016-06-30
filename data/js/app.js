(function($, window, document) {

	var options = {
		timeout: 5000,		// ajax timeout
		heartBeat: {
			interval: 10000	// heartbeat interval
		},
		sensors: {
			interval: 30000	// sensor interval
		},
		messages: {
			timeout: 10000	// message fade interval
		},
		api: "",			// api url
		test: {
			// stage: "wifi"		// setup state for testing
			// stage: "middleware"	// setup state for testing
		}
	};

	var sparkdata = [],
		sparkline;

	function getUrlParams() {
		var vars = {};
		decodeURIComponent(window.location.search).replace(/[?&]+([^=&]+)=([^&]*)/gi, function(m,key,value) {
			vars[key] = value;
		});
		return vars;
	}

	function hashCode(str) {
		var hash = 0, i, chr, len;
		if (str.length === 0) return hash;
		for (i = 0, len = str.length; i < len; i++) {
			chr   = str.charCodeAt(i);
			hash  = ((hash << 5) - hash) + chr;
			hash |= 0; // Convert to 32bit integer
		}
		return hash;
	}

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
		return $.getJSON(getApi("/api/plugins")).done(function(json) {
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
				else if (plugin.name == "dht") {
					unit = "°C";
					plugin.title = "DHT";
					el.find(".description").html("DHT sensors are basic, ultra low-cost digital temperature and humidity sensors.");
				}
				else {
					unit = "";
					plugin.title = plugin.name;
				}
				el.find(".name, .title").text(plugin.title);

				if (json && json.length) {
					$(".state-plugins .not-configured").remove();
				}

				// add sensors
				$.each(plugin.sensors, function(j, sensor) {
					$(".state-sensors .not-configured").remove();

					// update sensor, check unit again
					sensor.plugin = plugin.name;
					sensor.unit = unit;
					getSensorType(sensor);

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
		el.data(sensor);
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

		// update monitor all button
		if ($(".state-sensors .sensor:not(.template)").length) {
			$(".state-sensors .sensor-frontend").removeClass("secondary");
		}
		else {
			$(".state-sensors .sensor-frontend").addClass("secondary");
		}

		$(".state-sensors .sensor:not(.template)").each(function(i, el) {
			console.log($(el).data());
		});
	}

	function connectSensor(sensor) {
		// 1) check if sensor already exists at middleware
		var deferredUuid = $.ajax(getMiddleware() + "/iot/" + sensor.hash + ".json").then(function(json) {
			if (json.entities  && json.entities.length === 1) {
				// exactly one match - resolve uuid
				return $.Deferred().resolveWith(this, [json.entities[0].uuid]);
			}
			if (json.entities === undefined) {
				// wrong mw version
				console.warn("FOOOOO"); // @TODO
				notify("warning", "Middleware failed", "Could not perform middleware operation. Check middleware version (needs d63104b).");
			}

			// 2) if not create channel
			var data = {
				operation: "add",
				type: getSensorType(sensor),
				title: sensor.addr,
				owner: sensor.hash,		// remember sensor hash
				style: "lines",
				resolution: 1
			};

			return $.ajax(getMiddleware() + "/channel.json?" + $.param(data)).then(function(json) {
				if (json.entity !== undefined && json.entity.uuid !== undefined) {
					return $.Deferred().resolveWith(this, [json.entity.uuid]);
				}
				notify("error", "Sensor not connected", "Could not connect sensor " + sensor.addr + " to the middleware. Middleware says: '" + json.exception.message + "'");
				return $.Deferred().reject();
			}, function() {
				notify("error", "Sensor not connected", "Could not connect sensor " + sensor.addr + " to the middleware.");
				return $.Deferred().reject();
			});
		}, function() {
			notify("error", "Middleware error", "Could not connect to middleware.");
		});

		// 3) update sensor config with uuid
		deferredUuid.done(function(uuid) {
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
				var msg = "Could not delete sensor " + sensor.addr + " from middleware. Sensor will be disconnected instead. ";
				msg += "Middleware says: '" + json.exception.message + "'";
				notify("error", "Sensor not deleted", msg);
			}

			// clear uuid from vzero
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
			notify("error", "Sensor not disconnected", "Could not disconnect sensor " + sensor.addr + " from the middleware.");
		});
	}

	function updateSensors() {
		$.ajax(getApi("/api/plugins"), {
			timeout: options.timeout
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

	function getSensorType(sensor) {
		switch (sensor.plugin) {
			case "1wire":
				return "temperature";
			case "dht":
				if (sensor.addr == "temp") {
					sensor.unit = "°C";
					return "temperature";
				}
				sensor.unit = "%";
				return "humidity";
			case "analog":
				return "voltage";
			case "wifi":
				return "rssi";
			default:
				return "powersensor";
		}
	}

	function getApi(api) {
		return options.api + api;
	}

	function getSensorApi(data) {
		return getApi("/api/" + data.plugin + "/" + data.addr);
	}

	function getMiddleware() {
		var mw = $(".middleware").val();
		if (!mw) {
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

	function heartBeat(initial) {
		return $.ajax(getApi('/api/status' + (initial ? "?initial=1" : "")), {
			timeout: options.timeout
		})
		.done(function(json) {
			// [0: "DEFAULT", 1: "WDT", 2: "EXCEPTION", 3: "SOFT_WDT", 4: "SOFT_RESTART", 5: "DEEP_SLEEP_AWAKE", 6: "EXT_SYS_RST"]
			if (json.resetcode == 1 || json.resetcode == 3) {
				notify("error", "Unexpected restart", "The VZero has experienced an unexpected restart, triggered by the built-in watch dog timer.");
			}
			else if (json.resetcode == 2) {
				notify("error", "Unexpected restart", "The VZero has experienced an unexpected restart, caused by an exception.");
			}
			else if (json.resetcode == 4 && json.uptime < 30000) {
				notify("warning", "Restart", "The VZero was restarted.");
			}

			var heap = json.heap;
			if (heap < 8192) {
				notify("warning", "Low memory", "Available memory has reached a critical limit. VZero might become unstable.");
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
			return $.Deferred().resolveWith(this, [json]);
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
		heartBeat(true).done(function(json) {
			$('.loader').remove();
			$('.content > *').unwrap();

			var addr = "http://vzero-" + json.serial + ".local";
			$("a.address").text(addr);
			$("a.address").attr("href", addr);

			$('title').text($('title').text() + " (" + json.serial + ")");
			$('.ip').text(json.ip);
			$('.wifimode').text(json.wifimode);
			$('.serial').text(json.serial);
			$('.build').text(json.build);
			$('.ssid').val(json.ssid);
			$('.pass').val(json.pass);
			$('.middleware').val(json.middleware);

			// initial setup - wifi
			if ($(".wifimode").text() == "Access Point" || options.test.stage == "wifi") {
				$(".column.row:not(.state-always), .menu-container").addClass("hide");
				$(".state-initial-wifi").removeClass("hide");
			}
			// initial setup - middleware
			else if ($(".middleware").val().trim() === "" || options.test.stage == "middleware") {
				$(".middleware").val("http://demo.volkszaehler.org/middleware.php");
				$(".column.row:not(.state-always), .menu-container").addClass("hide");
				$(".state-initial-middleware").removeClass("hide");
			}
			else {
				menu(window.location.hash.substr(1) || "home");
				$(".menu a").click(function() {
					menu($(this).attr("href").slice(1));
				});

				sparkline = new Sparkline($(".spark")[0], {
					height: 30,
					width: $(".spark").width(),
					lineColor: "#666",
					endColor: "#0292C0",
					min: 0
				});

				// read plugins and setup sensor update
				initializePlugins().done(function() {
					window.setInterval(function() {
						updateSensors();
					}, options.sensors.interval);
				});

				// setup heartbeat update
				window.setInterval(function() {
					heartBeat();
				}, options.heartBeat.interval);

				window.setInterval(function() {
					var now = Date.now();
					$(".messages .row").each(function(i, el) {
						if (now - $(el).data().created > options.messages.timeout) {
							$(el).fadeOut("slow", function() {
								$(el).remove();
								if ($(".messages .row").length === 0) {
									$(".messages").addClass("hide");
								}
							});
						}
					});
				}, 2000);
			}
		}).fail(function() {
			window.setTimeout(connectDevice, 200);
		});
	}

	// dom ready
	$(function() {
		// url parameters
		var params = getUrlParams();
		$.extend(options, params);
		console.warn(options);

		// js loaded - update UI
		$('.loader .subheader').text("connecting...");
		connectDevice();
	});
})(window.jQuery, window, document);
