var tw = {
  // Constants
  // Tile flags
  TILEFLAG_VFLIP: 1,
  TILEFLAG_HFLIP: 2,
  TILEFLAG_OPAQUE: 4,
  TILEFLAG_ROTATE: 8,

  LAYERTYPE_INVALID: 0,
  LAYERTYPE_GAME: 1,
  LAYERTYPE_TILES: 2,
  LAYERTYPE_QUADS: 3,

  // TILESLAYERFLAG_* из mapitems.h: игровой слой и слои сущностей.
  TILESLAYERFLAG_GAME: 1,
  // Индекс тайла «спавн игрока» в игровом слое.
  TILE_SPAWN: 192,

  MAPITEMTYPE_VERSION: 0,
  MAPITEMTYPE_INFO: 1,
  MAPITEMTYPE_IMAGE: 2,
  MAPITEMTYPE_ENVELOPE: 3,
  MAPITEMTYPE_GROUP: 4,
  MAPITEMTYPE_LAYER: 5,
  MAPITEMTYPE_ENVPOINTS: 6,
  MAPITEMTYPE_UUID_INDEX: 0xffff,

  // UUIDv3("mapitemtype-group@ddnet.tw") stored as 4 big-endian int32s (CItemEx).
  GROUP_EX_UUID_INTS: [1244056095, 1690120419, -1871024002, -1395527552],

  IMAGE_FORMAT_RGB: 0,
  IMAGE_FORMAT_RGBA: 1,

  // CURVETYPE_* из mapitems.h.
  CURVETYPE_STEP: 0,
  CURVETYPE_LINEAR: 1,
  CURVETYPE_SLOW: 2,
  CURVETYPE_FAST: 3,
  CURVETYPE_SMOOTH: 4,
  CURVETYPE_BEZIER: 5,
}

tw.config = window.twConfig || {};
// Время, на котором вычисляются конверты (день/ночь, туман, облака).
// 0 — статичный кадр: карту можно рисовать только по запросу и не крутить
// анимацию в фоне (см. tw.requestRender).
tw.envelopeTime = typeof tw.config.envelopeTime === "number" ? tw.config.envelopeTime : 0;
// Рисовать сразу после изменения камеры. false — только по tw.requestRender().
tw.autoRender = tw.config.autoRender !== false;
tw.mat4 = window.glMatrix && window.glMatrix.mat4 ? window.glMatrix.mat4 : window.mat4;
tw.vec4 = window.glMatrix && window.glMatrix.vec4 ? window.glMatrix.vec4 : window.vec4;
tw.ZOOM_MIN = typeof tw.config.zoomMin === "number" ? tw.config.zoomMin : 0.2;
tw.ZOOM_MAX = typeof tw.config.zoomMax === "number" ? tw.config.zoomMax : 6.0;
tw.ZOOM_WHEEL_FACTOR = typeof tw.config.zoomWheelFactor === "number" ? tw.config.zoomWheelFactor : 10;
tw.ZOOM_SCROLL_GAIN = typeof tw.config.zoomScrollGain === "number" ? tw.config.zoomScrollGain : 0.05;
tw.ZOOM_WHEEL_PIXEL_QUANTUM = typeof tw.config.zoomWheelPixelQuantum === "number" ? tw.config.zoomWheelPixelQuantum : 120;
tw.ZOOM_WHEEL_STEP = typeof tw.config.zoomWheelStep === "number" ? tw.config.zoomWheelStep : 1.08;
tw.ZOOM_WHEEL_MAX_STEPS_PER_EVENT = typeof tw.config.zoomWheelMaxStepsPerEvent === "number" ? tw.config.zoomWheelMaxStepsPerEvent : 8;
tw.ZOOM_WHEEL_LINE_HEIGHT = typeof tw.config.zoomWheelLineHeight === "number" ? tw.config.zoomWheelLineHeight : 16;
tw.ZOOM_WHEEL_LOW_DELTA_GAMMA = typeof tw.config.zoomWheelLowDeltaGamma === "number" ? tw.config.zoomWheelLowDeltaGamma : 0.55;
tw.EXTERNAL_IMAGE_BASE_URL = typeof tw.config.localImageUrl === "string" ? tw.config.localImageUrl : "https://preview.ddhost.cc/i/";
tw.MOVE_SMOOTHING = typeof tw.config.moveSmoothing === "number" ? tw.config.moveSmoothing : 0.22;
tw.MOVE_ACCELERATION = typeof tw.config.moveAcceleration === "number" ? tw.config.moveAcceleration : 0.35;
tw.MOVE_DECELERATION = typeof tw.config.moveDeceleration === "number" ? tw.config.moveDeceleration : 0.86;
tw.KEYBOARD_MOVE_SPEED_MIN = typeof tw.config.keyboardMoveSpeedMin === "number" ? tw.config.keyboardMoveSpeedMin : 6;
tw.KEYBOARD_MOVE_SPEED_MAX = typeof tw.config.keyboardMoveSpeedMax === "number" ? tw.config.keyboardMoveSpeedMax : 18;
tw.KEYBOARD_MOVE_RAMP_MS = typeof tw.config.keyboardMoveRampMs === "number" ? tw.config.keyboardMoveRampMs : 900;

tw.keyState = {};

tw.isMoveKeyPressed = function (keys) {
  for (var i = 0; i < keys.length; i++) {
    if (tw.keyState[keys[i]])
      return true;
  }
  return false;
}

tw.getKeyboardMove = function () {
  var dx = 0;
  var dy = 0;

  if (tw.isMoveKeyPressed(["a", "q", "arrowleft"]))
    dx -= 1;
  if (tw.isMoveKeyPressed(["d", "arrowright"]))
    dx += 1;
  if (tw.isMoveKeyPressed(["w", "z", "arrowup"]))
    dy -= 1;
  if (tw.isMoveKeyPressed(["s", "arrowdown"]))
    dy += 1;

  if (dx !== 0 && dy !== 0) {
    var inv = Math.SQRT1_2;
    dx *= inv;
    dy *= inv;
  }

  return [dx, dy];
}

tw.getKeyboardSpeed = function () {
  if (tw.KEYBOARD_MOVE_RAMP_MS <= 0)
    return tw.KEYBOARD_MOVE_SPEED_MAX;

  var t = tw.keyboardHoldMs / tw.KEYBOARD_MOVE_RAMP_MS;
  if (t > 1)
    t = 1;
  if (t < 0)
    t = 0;

  return tw.KEYBOARD_MOVE_SPEED_MIN + (tw.KEYBOARD_MOVE_SPEED_MAX - tw.KEYBOARD_MOVE_SPEED_MIN) * t;
}

tw.updateZoomAnchor = function (event) {
  if (!tw.canvas || !event)
    return;

  var rect = tw.canvas.getBoundingClientRect();
  if (rect.width <= 0 || rect.height <= 0)
    return;

  var x = (event.clientX - rect.left) / rect.width;
  var y = (event.clientY - rect.top) / rect.height;

  if (x < 0) x = 0;
  if (x > 1) x = 1;
  if (y < 0) y = 0;
  if (y > 1) y = 1;

  tw.zoomAnchor[0] = x;
  tw.zoomAnchor[1] = y;
}

tw.clampZoom = function (value) {
  if (value < tw.ZOOM_MIN)
    return tw.ZOOM_MIN;
  if (value > tw.ZOOM_MAX)
    return tw.ZOOM_MAX;
  return value;
}

tw.resetCameraToInitial = function () {
  tw.cameraPos[0] = tw.initialCameraPos[0];
  tw.cameraPos[1] = tw.initialCameraPos[1];
  tw.cameraPosTarget[0] = tw.initialCameraPos[0];
  tw.cameraPosTarget[1] = tw.initialCameraPos[1];
  tw.cameraZoom = tw.initialCameraZoom;
  tw.cameraZoomTarget = tw.initialCameraZoom;
  tw.moveVelocity[0] = 0;
  tw.moveVelocity[1] = 0;
  tw.mouseDownInc[0] = 0;
  tw.mouseDownInc[1] = 0;
  tw.keyboardHoldMs = 0;
}

tw.init = function (attrs) {
  tw.canvas = document.getElementById("cnvs");

  // Change background color of canvas to black
  // to avoid white border artefacts
  tw.canvas.style.background = "#000000"

  // PATCH (Editor Studio): вместо alert() — та же функция ошибок, что и при
  // загрузке карты: окно показывает сообщение, страница не «залипает».
  var fail = function (code) {
    if (typeof tw.onMapError === "function")
      tw.onMapError(code);
  };

  try {
    var glAttrs = { antialias: false, alpha: false, depth: false, stencil: false, powerPreference: "high-performance" };
    tw.gl = tw.canvas.getContext("webgl2", glAttrs) || tw.canvas.getContext("webgl", glAttrs) || tw.canvas.getContext("experimental-webgl", glAttrs);
  } catch (e) {
    fail("no_webgl");
    return; // Failed to initialize webgl :/
  }

  if (!tw.gl) {
    fail("no_webgl");
    return;
  }

  tw.isWebGL2 = (typeof WebGL2RenderingContext !== "undefined" && tw.gl instanceof WebGL2RenderingContext) ||
    typeof tw.gl.texStorage2D === "function";

  tw.dirty = true;
  tw.renderFrames = 2;
  tw.textureFailures = [];

  // Enable GL_BLEND for transparent textures
  tw.gl.enable(tw.gl.BLEND);
  tw.gl.blendFunc(tw.gl.SRC_ALPHA, tw.gl.ONE_MINUS_SRC_ALPHA);

  // Init shaders
  tw.stdShader = tw.gl.createProgram();

  var fShader = tw.buildShader(tw.fragmentShader, tw.gl.FRAGMENT_SHADER);
  var vShader = tw.buildShader(tw.vertexShader, tw.gl.VERTEX_SHADER);

  if (!fShader || !vShader) {
    fail("shader_failed");
    return;
  }

  tw.gl.attachShader(tw.stdShader, fShader);
  tw.gl.attachShader(tw.stdShader, vShader);
  tw.gl.linkProgram(tw.stdShader);

  if (!tw.gl.getProgramParameter(tw.stdShader, tw.gl.LINK_STATUS)) {
    console.error("[map] шейдер не собран:", tw.gl.getProgramInfoLog(tw.stdShader));
    fail("shader_failed");
    return;
  }

  tw.gl.useProgram(tw.stdShader);

  // Attribute / uniform references
  // Vertex position attribute
  tw.stdShader.vPosAttr = tw.gl.getAttribLocation(tw.stdShader, "aPosition");
  tw.gl.enableVertexAttribArray(tw.stdShader.vPosAttr);
  // Vertex color attribute
  tw.stdShader.vVertexColor = tw.gl.getAttribLocation(tw.stdShader, "aVertexColor");
  // Tex coord attribute
  tw.stdShader.vTexCoordAttr = tw.gl.getAttribLocation(tw.stdShader, "aTexCoord");
  // Move / projection matrix
  tw.stdShader.pMatUni = tw.gl.getUniformLocation(tw.stdShader, "uPMatrix");
  tw.stdShader.mvMatUni = tw.gl.getUniformLocation(tw.stdShader, "uMVMatrix");
  // Texture sampler
  tw.stdShader.uSampler = tw.gl.getUniformLocation(tw.stdShader, "uSampler");
  // Frag color mask
  tw.stdShader.uColorMask = tw.gl.getUniformLocation(tw.stdShader, "uColorMask");
  tw.gl.uniform4fv(tw.stdShader.uColorMask, [1.0, 1.0, 1.0, 1.0]); // Reset
  // Using texture coords
  tw.stdShader.uTexCoord = tw.gl.getUniformLocation(tw.stdShader, "uTexCoord");
  tw.gl.uniform1i(tw.stdShader.uTexCoord, 0); // Reset
  // Using vertex color
  tw.stdShader.uVertexColor = tw.gl.getUniformLocation(tw.stdShader, "uVertexColor");
  tw.gl.uniform1i(tw.stdShader.uVertexColor, 0); // Reset

  tw.gl.clearColor(0.05, 0.05, 0.07, 1.0);

  tw.aspect = 1;

  // Matrices
  tw.prjMat = tw.mat4.create();
  tw.mvMat = tw.mat4.create();

  // Temp objects
  tw.tmpMat = tw.mat4.create();
  tw.tmpVec4 = tw.vec4.create();
  tw.tmpBuf = tw.gl.createBuffer();

  // Camera
  tw.cameraPos = [0.0, 0.0]
  tw.cameraPosTarget = [0.0, 0.0]
  tw.cameraZoom = 1.0
  tw.cameraZoomTarget = tw.cameraZoom
  tw.initialCameraPos = [0.0, 0.0]
  tw.initialCameraZoom = tw.cameraZoom
  tw.zoomAnchor = [0.5, 0.5]

  // Mapscreen
  tw.mapScreen = tw.vec4.create();
  tw.worldView = [1000, 1000];

  tw.screenResize();
  tw.canvas.addEventListener("webglcontextlost", function (e) {
    e.preventDefault();
    fail("context_lost");
  });
  window.addEventListener("resize", function () {
    tw.requestRender(2);
  });

  tw.mousePressed = false
  tw.mouseDownPos = [0.0, 0.0]
  tw.mouseLastPos = [0.0, 0.0]
  tw.mouseDownInc = [0.0, 0.0]
  tw.moveVelocity = [0.0, 0.0]
  tw.keyboardHoldMs = 0
  tw.zoomWheelAccum = 0
  tw.lastMainLoopTs = Date.now()

  // Mouse events
  $("#cnvs").mousedown(function (e) {
    tw.mousePressed = true;
    tw.requestRender(2);
    tw.mouseDownPos[0] = e.clientX;
    tw.mouseDownPos[1] = e.clientY;

    tw.mouseDownInc[0] = 0;
    tw.mouseDownInc[1] = 0;
  });

  $(document).on("blur mouseup", function (e) {
    tw.mousePressed = false;
    tw.requestRender(2);
  });

  $(document).on("keydown", function (e) {
    var key = (e.key || "").toLowerCase();
    tw.keyState[key] = true;
    tw.requestRender(2);
    if (key === " " || key === "space" || key === "spacebar") {
      tw.resetCameraToInitial();
      e.preventDefault();
      return;
    }
    if (["arrowup", "arrowdown", "arrowleft", "arrowright"].indexOf(key) !== -1)
      e.preventDefault();
  });

  $(document).on("keyup", function (e) {
    var key = (e.key || "").toLowerCase();
    tw.keyState[key] = false;
    tw.requestRender(2);
  });

  $("#cnvs").mousemove(function (e) {
    tw.updateZoomAnchor(e);

    if (tw.mousePressed) {
      tw.requestRender(2);
      tw.mouseDownInc[0] += tw.mouseLastPos[0] - e.clientX;
      tw.mouseDownInc[1] += tw.mouseLastPos[1] - e.clientY;
    }

    tw.mouseLastPos[0] = e.clientX;
    tw.mouseLastPos[1] = e.clientY;
  });

  $("#cnvs").mousewheel(function (event, delta, deltaX, deltaY) {
    tw.updateZoomAnchor(event);

    var orgEvent = event.originalEvent || event;
    var pixelDelta = 0;

    if (typeof orgEvent.deltaY === "number" && isFinite(orgEvent.deltaY)) {
      pixelDelta = -orgEvent.deltaY;
      if (orgEvent.deltaMode === 1)
        pixelDelta *= tw.ZOOM_WHEEL_LINE_HEIGHT;
      else if (orgEvent.deltaMode === 2)
        pixelDelta *= (tw.canvas && tw.canvas.height) ? tw.canvas.height : window.innerHeight;
    } else if (typeof orgEvent.wheelDeltaY === "number" && isFinite(orgEvent.wheelDeltaY)) {
      pixelDelta = orgEvent.wheelDeltaY;
    } else if (typeof orgEvent.wheelDelta === "number" && isFinite(orgEvent.wheelDelta)) {
      pixelDelta = orgEvent.wheelDelta;
    } else if (typeof orgEvent.detail === "number" && isFinite(orgEvent.detail)) {
      pixelDelta = orgEvent.detail * -40;
    } else {
      var fallbackDeltaY = Number(deltaY);
      if (isFinite(fallbackDeltaY) && fallbackDeltaY !== 0)
        pixelDelta = fallbackDeltaY * tw.ZOOM_WHEEL_PIXEL_QUANTUM;
      else {
        var fallbackDelta = Number(delta);
        if (isFinite(fallbackDelta))
          pixelDelta = fallbackDelta * tw.ZOOM_WHEEL_PIXEL_QUANTUM;
      }
    }

    if (!isFinite(pixelDelta) || pixelDelta === 0)
      return;

    var quantum = tw.ZOOM_WHEEL_PIXEL_QUANTUM;
    if (!isFinite(quantum) || quantum <= 0)
      quantum = 120;

    var gamma = tw.ZOOM_WHEEL_LOW_DELTA_GAMMA;
    if (!isFinite(gamma) || gamma <= 0 || gamma >= 1)
      gamma = 0.55;

    var absDelta = Math.abs(pixelDelta);
    if (absDelta > 0 && absDelta < quantum)
      pixelDelta = (pixelDelta < 0 ? -1 : 1) * quantum * Math.pow(absDelta / quantum, gamma);

    var maxSteps = Math.max(1, Math.floor(tw.ZOOM_WHEEL_MAX_STEPS_PER_EVENT));
    var maxPixels = quantum * maxSteps;
    if (pixelDelta > maxPixels)
      pixelDelta = maxPixels;
    if (pixelDelta < -maxPixels)
      pixelDelta = -maxPixels;

    var zoomFactor = Math.pow(tw.ZOOM_WHEEL_STEP, pixelDelta / quantum);
    tw.cameraZoomTarget = tw.clampZoom(tw.cameraZoomTarget * zoomFactor);
    tw.requestRender(3);
    tw.zoomWheelAccum = 0;
    tw.zoomed = true;
  });

  tw.zoomed = false

  // Touch (even multi)

  tw.touchScaling = false;
  tw.touchDistance = 0;

  $("#cnvs").on("touchstart", function (e) {
    var e = e.originalEvent;
    tw.mousePressed = true;
    tw.requestRender(2);
    tw.mouseLastPos[0] = e.touches[0].clientX;
    tw.mouseLastPos[1] = e.touches[0].clientY;
    /* pinch */
    if (e.touches.length === 2) {
      tw.touchScaling = true;
      tw.touchDistance = getDistance(e);
    }
  });
  $("#cnvs").on("touchmove", function (e) {
    var e = e.originalEvent;
    if (tw.mousePressed) {
      tw.requestRender(2);
      tw.mouseDownInc[0] += tw.mouseLastPos[0] - e.touches[0].clientX;
      tw.mouseDownInc[1] += tw.mouseLastPos[1] - e.touches[0].clientY;
    }
    tw.mouseLastPos[0] = e.touches[0].clientX;
    tw.mouseLastPos[1] = e.touches[0].clientY;
    /* pinch */
    if (tw.touchScaling) {
      var prev = tw.touchDistance;
      tw.touchDistance = getDistance(e);
      tw.cameraZoomTarget = tw.clampZoom((tw.touchDistance / prev) * tw.cameraZoomTarget);
      tw.requestRender(3);
    }
  });

  $(document).on("touchend", function (e) {
    tw.mousePressed = false;
    tw.requestRender(2);
    /* pinch */
    tw.touchScaling = false;
  });

  function getDistance(e) {
    return Math.sqrt(
      (e.touches[0].clientX - e.touches[1].clientX) * (e.touches[0].clientX - e.touches[1].clientX) +
      (e.touches[0].clientY - e.touches[1].clientY) * (e.touches[0].clientY - e.touches[1].clientY));
  }

  var me = this;

  if (attrs.mapData) {
    this.map = new tw.Map(attrs.mapData);
    tw.mainLoop();
    if (typeof tw.onMapLoaded === "function") tw.onMapLoaded(this.map);
  } else if (attrs.mapUrl) {
    var mapUrl = attrs.mapUrl;
    // PATCH (Editor Studio): адрес карты приходит из api/map.php с параметрами
    // (?action=file&world=3) — расширение дописываем только к «голому» имени,
    // иначе параметр мира превращался бы в «1.map».
    if (mapUrl.indexOf("?") === -1 && !/\.map($|[?#])/i.test(mapUrl))
      mapUrl += ".map";

    var req = new XMLHttpRequest();
    req.open("GET", mapUrl, true);
    req.responseType = "arraybuffer";
    req.timeout = 20000;

    // PATCH (Editor Studio): понятная ошибка вместо молчания — редактор
    // показывает её в окне выбора точки и предлагает «Повторить».
    req.onload = function (evt) {
      var status = evt.target.status;
      if (status < 200 || status >= 300 || !evt.target.response || evt.target.response.byteLength === 0) {
        // PATCH (Editor Studio): api/map.php объясняет причину в X-Map-Error
        // (нет каталога карт, нет файла мира) — показываем её как есть.
        var code = "http_" + status;
        try {
          var header = evt.target.getResponseHeader("X-Map-Error");
          if (header)
            code = header;
        } catch (err) { /* заголовок недоступен */ }
        tw.mapLoadError(code);
        return;
      }
      try {
        me.map = new tw.Map(evt.target.response);
      } catch (err) {
        tw.mapLoadError("parse_failed");
        return;
      }
      tw.mainLoop();
      if (typeof tw.onMapLoaded === "function") tw.onMapLoaded(me.map);
    }

    req.onerror = function () {
      tw.mapLoadError("network");
    }
    req.ontimeout = function () {
      tw.mapLoadError("timeout");
    }

    req.send(null);
  }
}

// PATCH (Editor Studio): точка входа для сообщений об ошибках загрузки карты.
tw.mapLoadError = function (code) {
  if (typeof tw.onMapError === "function")
    tw.onMapError(code);
}

// Enable / Disable shader flags
tw.sTexCoords = function (state) {
  if (state)
    tw.gl.enableVertexAttribArray(tw.stdShader.vTexCoordAttr);
  else
    tw.gl.disableVertexAttribArray(tw.stdShader.vTexCoordAttr);

  tw.gl.uniform1i(tw.stdShader.uTexCoord, state);
}

tw.sVertexColor = function (state) {
  if (state)
    tw.gl.enableVertexAttribArray(tw.stdShader.vVertexColor);
  else
    tw.gl.disableVertexAttribArray(tw.stdShader.vVertexColor);

  tw.gl.uniform1i(tw.stdShader.uVertexColor, state);
}


tw.setColorMask = function (rgba) {
  tw.gl.uniform4fv(tw.stdShader.uColorMask, rgba); // Reset
}

tw.setMapScreen = function (topLeftX, topLeftY, bottomRightX, bottomRightY) {
  tw.mapScreen[0] = topLeftX;
  tw.mapScreen[1] = topLeftY;
  tw.mapScreen[2] = bottomRightX;
  tw.mapScreen[3] = bottomRightY;
}

tw.getParams = function () {
  if (location.search === "") return {};
  var o = {}, nvPairs = location.search.substr(1).replace(/\+/g, " ").split("&");
  nvPairs.forEach(function (pair) {
    var e = pair.indexOf('=');
    var n = decodeURIComponent(e < 0 ? pair : pair.substr(0, e)),
      v = (e < 0 || e + 1 == pair.length) ? null :
        decodeURIComponent(pair.substr(e + 1, pair.length - e));
    if (!(n in o))
      o[n] = v;
    else if (o[n] instanceof Array)
      o[n].push(v);
    else
      o[n] = [o[n], v];
  });

  return o;
}

tw.parseMapGroup = function (groupData) {
  var obj = {}

  var d = new DataView(groupData);
  d.resetReader();

  obj.version = d.uint32();
  obj.offsX = d.int32();
  obj.offsY = d.int32();
  obj.parallaxX = d.int32();
  obj.parallaxY = d.int32();
  obj.startLayer = d.uint32();
  obj.numLayers = d.uint32();

  // CMapItemGroup v1 ends here. Clipping fields were added in v2; older map
  // items must stay valid and must never read beyond their data block.
  obj.useClipping = 0;
  obj.clipX = 0;
  obj.clipY = 0;
  obj.clipW = 0;
  obj.clipH = 0;
  if (obj.version >= 2) {
    if (groupData.byteLength < 48)
      throw new RangeError("Truncated map group clipping data");
    obj.useClipping = d.uint32();
    obj.clipX = d.int32();
    obj.clipY = d.int32();
    obj.clipW = d.int32();
    obj.clipH = d.int32();
  }

  return obj;
}

tw.parseMapLayer = function (layerData) {
  var obj = {}

  var d = new DataView(layerData);
  d.resetReader();

  obj.version = d.uint32();
  obj.type = d.uint32();
  obj.flags = d.uint32();

  return obj;
}

tw.parseMapLayerQuads = function (layerData) {
  var obj = {}

  var d = new DataView(layerData);
  d.resetReader();

  /*obj.version =*/
  d.uint32();
  /*obj.type =*/
  d.uint32();
  /*obj.flags =*/
  d.uint32();

  obj.version = d.uint32();
  obj.numQuads = d.uint32();
  obj.data = d.int32();
  obj.image = d.int32();

  return obj;
}

tw.parseMapLayerTiles = function (layerData) {
  var obj = {}

  var d = new DataView(layerData);
  d.resetReader();

  /*obj.version =*/
  d.uint32();
  /*obj.type =*/
  d.uint32();
  /*obj.flags =*/
  d.uint32();

  obj.version = d.uint32();
  obj.width = d.int32();
  obj.height = d.int32();
  obj.flags = d.uint32();

  obj.color = [d.uint32() & 0xff, d.uint32() & 0xff, d.uint32() & 0xff, d.uint32() & 0xff]

  obj.colorEnv = d.int32();
  obj.colorEnvOffset = d.int32();

  obj.image = d.int32();
  obj.data = d.int32();

  //TODO: name

  return obj;
}

tw.findExItemType = function (dFile, uuidInts) {
  if (!dFile || typeof dFile.getType !== "function" || !uuidInts || uuidInts.length < 4)
    return undefined;
  var idxInfo = dFile.getType(tw.MAPITEMTYPE_UUID_INDEX);
  if (!idxInfo)
    return undefined;
  for (var i = 0; i < idxInfo.num; i++) {
    var item = dFile.getItem(idxInfo.start + i);
    if (!item || !item.data || item.data.byteLength < 16)
      continue;
    var d = new DataView(item.data);
    if (
      d.getInt32(0, true) === uuidInts[0] &&
      d.getInt32(4, true) === uuidInts[1] &&
      d.getInt32(8, true) === uuidInts[2] &&
      d.getInt32(12, true) === uuidInts[3]
    ) {
      return item.id;
    }
  }
  return undefined;
}

tw.parseMapGroupEx = function (groupExData) {
  if (!groupExData || groupExData.byteLength < 8)
    return undefined;
  var d = new DataView(groupExData);
  d.resetReader();
  var version = d.int32();
  if (version < 1)
    return undefined;
  var parallaxZoom = d.int32();
  if (parallaxZoom < 0) parallaxZoom = 0;
  if (parallaxZoom > 100) parallaxZoom = 100;
  return { version: version, parallaxZoom: parallaxZoom };
}

/**
 * Значение конверта — как в DDNet (CRenderMap::RenderEvalEnvelope).
 * Поддерживает все стандартные типы кривых (STEP, LINEAR, SLOW, FAST, SMOOTH,
 * BEZIER) и записывает только каналы конверта, сохраняя умолчания вызывающего
 * кода ([0,0,0,0] для позиции, [1,1,1,1] для цвета).
 */
tw.evalEnvelope = function (map, index, offsetMs, result) {
  for (var initIdx = 0; initIdx < 4; initIdx++) {
    if (result[initIdx] === undefined)
      result[initIdx] = 1;
  }
  if (!map || !map.envelopes || index < 0 || index >= map.envelopes.length)
    return result;

  var env = map.envelopes[index];
  var points = map.envelopePoints;
  if (!env || env.num <= 0) {
    result[0] = 0; result[1] = 0; result[2] = 0; result[3] = 0;
    return result;
  }
  if (!points || env.start < 0 || env.start >= points.length)
    return result;

  var channels = env.channels == null ? 4 : Math.max(0, Math.min(4, env.channels));
  var first = points[env.start];
  var last = points[env.start + env.num - 1];
  if (!last) {
    last = first;
  }

  if (env.num === 1) {
    for (var c0 = 0; c0 < channels; c0++)
      result[c0] = first.values[c0] / 1024;
    return result;
  }

  var span = last.time;
  var t = (tw.envelopeTime || 0) + (offsetMs || 0);
  if (span > 0) {
    t = t % span;
    if (t < 0) t += span;
  } else {
    t = 0;
  }

  var prev = first;
  var next = first;
  if (t >= last.time) {
    prev = last;
    next = last;
  } else if (t > first.time) {
    next = last;
    for (var i = 0; i < env.num - 1; i++) {
      var a = points[env.start + i];
      var b = points[env.start + i + 1];
      if (a && b && t >= a.time && t < b.time) {
        prev = a;
        next = b;
        break;
      }
    }
  }

  var delta = next.time - prev.time;
  var k = 0;
  if (delta > 0) {
    k = (t - prev.time) / delta;
    if (k < 0) k = 0;
    if (k > 1) k = 1;
  }

  if (prev.curve === tw.CURVETYPE_STEP) {
    k = 0;
  } else if (prev.curve === tw.CURVETYPE_SLOW) {
    k = k * k * k;
  } else if (prev.curve === tw.CURVETYPE_FAST) {
    var invK = 1 - k;
    k = 1 - invK * invK * invK;
  } else if (prev.curve === tw.CURVETYPE_SMOOTH) {
    k = 3 * k * k - 2 * k * k * k;
  }

  for (var c = 0; c < channels; c++) {
    var v0 = prev.values[c] / 1024;
    var v1 = next.values[c] / 1024;
    if (prev.curve === tw.CURVETYPE_BEZIER && delta > 0 && prev.outTangentdy && next.inTangentdy) {
      var u = 1 - k;
      var p1 = v0 + prev.outTangentdy[c] / 1024;
      var p2 = v1 + next.inTangentdy[c] / 1024;
      result[c] = u * u * u * v0 + 3 * u * u * k * p1 + 3 * u * k * k * p2 + k * k * k * v1;
    } else {
      result[c] = v0 + (v1 - v0) * k;
    }
  }
  return result;
}

/**
 * Разбирает блок квадов по формату CQuad: 152 байта на квад.
 * В части карт DDNet пустой квад-слой (num === 0) хранит фиктивный буфер
 * на один квад (152 байта) — для num === 0 сразу возвращаем [].
 */
tw.parseLayerQuads = function (layerData, num) {
  var quads = [];
  var hasBuffer = layerData && (layerData.byteLength !== undefined || layerData.length !== undefined);
  var total = layerData && layerData.byteLength !== undefined
    ? layerData.byteLength
    : (layerData && layerData.length) || 0;
  var stride = 152; // CQuad: 5 points + 4 colors + 4 texcoords + 4 envelope ints.

  if (!hasBuffer || !Number.isSafeInteger(num) || num < 0)
    throw new RangeError("Invalid quad data size");
  if (num === 0)
    return quads;
  if (total !== num * stride)
    throw new RangeError("Invalid quad data size");

  var d = new DataView(layerData);
  d.resetReader();

  for (var q = 0; q < num; q++) {
    var obj = {};
    quads.push(obj);

    obj.points = []
    for (var i = 0; i < 5; i++) {
      obj.points.push({
        x: d.int32(),
        y: d.int32()
      });
    }

    obj.colors = []
    for (var c = 0; c < 4; c++) {
      obj.colors.push({
        r: d.int32() & 0xff,
        g: d.int32() & 0xff,
        b: d.int32() & 0xff,
        a: d.int32() & 0xff
      });
    }

    obj.texCoords = []
    for (var t = 0; t < 4; t++) {
      obj.texCoords.push({
        x: d.int32(),
        y: d.int32()
      });
    }

    obj.posEnv = d.int32();
    obj.posEnvOffset = d.int32();
    obj.colorEnv = d.int32();
    obj.colorEnvOffset = d.int32();

  }

  return quads;
}

// Editor-only compact gameplay data: one byte per cell, no per-tile objects.
// Supports both uncompressed tiles (version < 4) and DDNet m_Skip RLE (version >= 4).
tw.parseGameplayIndices = function (tileData, num, version) {
  if (!tileData || !Number.isSafeInteger(num) || num < 0)
    throw new RangeError("Truncated or invalid gameplay tile data");
  var bytes = new Uint8Array(tileData);
  var srcCount = Math.floor(bytes.byteLength / 4);
  if (version >= 4) {
    var rleIndices = new Uint8Array(num);
    var dstIdx = 0;
    var srcIdx = 0;
    while (dstIdx < num && srcIdx < srcCount) {
      var idxVal = bytes[srcIdx * 4];
      var skip = bytes[srcIdx * 4 + 2];
      for (var rep = 0; rep <= skip && dstIdx < num; rep++) {
        rleIndices[dstIdx++] = idxVal;
      }
      srcIdx++;
    }
    if (dstIdx < num)
      throw new RangeError("Truncated or invalid gameplay tile data");
    return rleIndices;
  }
  if (num > srcCount)
    throw new RangeError("Truncated or invalid gameplay tile data");
  var indices = new Uint8Array(num);
  for (var i = 0; i < num; i++) indices[i] = bytes[i * 4];
  return indices;
}

tw.parseLayerTiles = function (tileData, num, version) {
  var tiles = [];
  if (!tileData || num <= 0)
    return tiles;
  var bytes = new Uint8Array(tileData);
  var srcCount = Math.floor(bytes.byteLength / 4);

  if (version >= 4) {
    var srcIdx = 0;
    while (tiles.length < num && srcIdx < srcCount) {
      var base = srcIdx * 4;
      var index = bytes[base];
      var flags = bytes[base + 1];
      var skip = bytes[base + 2];
      for (var rep = 0; rep <= skip && tiles.length < num; rep++) {
        tiles.push({ index: index, flags: flags });
      }
      srcIdx++;
    }
    while (tiles.length < num)
      tiles.push({ index: 0, flags: 0 });
    return tiles;
  }

  var d = new DataView(tileData);
  d.resetReader();

  for (var i = 0; i < num; i++) {
    tiles.push({
      index: d.uint8(),
      flags: d.uint8()
    })

    // skip, reserved
    d.uint8();
    d.uint8();
  }

  return tiles;
}


// CEnvPoint: время (int, мс), тип кривой, 4 значения (22.10 fixed point).
tw.ENVPOINT_SIZE = 24;
// CEnvPoint_bezier (envelope version >= 3): CEnvPoint + 4*4 касательных int32.
tw.ENVPOINT_BEZIER_SIZE = 88;

tw.parseMapEnvelope = function (layerData) {
  var d = new DataView(layerData);
  d.resetReader();

  return {
    version: d.int32(),
    channels: d.int32(),
    start: d.int32(),
    num: d.int32()
  };
}

tw.parseEnvelopePoints = function (pointsData, envelopes) {
  var raw = new Uint8Array(pointsData);
  var d = new DataView(pointsData);
  d.resetReader();

  var useBezier = false;
  if (envelopes && envelopes.length) {
    for (var e = 0; e < envelopes.length; e++) {
      if (envelopes[e] && envelopes[e].version >= 3) {
        useBezier = true;
        break;
      }
    }
  }

  var stride = useBezier ? tw.ENVPOINT_BEZIER_SIZE : tw.ENVPOINT_SIZE;
  var points = [];
  var num = Math.floor(raw.byteLength / stride);
  for (var i = 0; i < num; i++) {
    var pt = {
      time: d.int32(),
      curve: d.int32(),
      values: [d.int32(), d.int32(), d.int32(), d.int32()]
    };
    if (useBezier) {
      pt.inTangentdx = [d.int32(), d.int32(), d.int32(), d.int32()];
      pt.inTangentdy = [d.int32(), d.int32(), d.int32(), d.int32()];
      pt.outTangentdx = [d.int32(), d.int32(), d.int32(), d.int32()];
      pt.outTangentdy = [d.int32(), d.int32(), d.int32(), d.int32()];
    }
    points.push(pt);
  }
  return points;
}

tw.parseMapImage = function (layerData) {
  var obj = {}

  var d = new DataView(layerData);
  d.resetReader();

  obj.version = d.uint32();
  obj.width = d.int32();
  obj.height = d.int32();
  obj.external = d.uint32();
  obj.imageName = d.int32();
  obj.imageData = d.int32();
  obj.format = (obj.version >= 2 && layerData.byteLength >= 28)
    ? d.int32()
    : tw.IMAGE_FORMAT_RGBA;

  return obj;
}

tw.getCString = function (buf) {
  var len;

  buf = new Uint8Array(buf);

  for (len = 0; len < buf.byteLength; len++) {
    if (buf[len] == 0)
      break;
  }

  return String.fromCharCode.apply(null, buf.subarray(0, len));
}

tw.Map = function (mapData) {
  this.textures = []; // loaded textures
  this.groups = [];
  this.envelopes = [];
  this.envelopePoints = [];
  this.textureFailures = tw.textureFailures;

  this.dataFile = new TwDataFile("", mapData)
  if (!this.dataFile.parse())
    throw new Error("Invalid map data file");

  var dFile = this.dataFile;

  // Конверты: без них фон рисуется белым — цвет слоёв задаётся именно ими.
  var envInfo = dFile.getType(tw.MAPITEMTYPE_ENVELOPE);
  if (envInfo != undefined) {
    for (var e = 0; e < envInfo.num; e++) {
      this.envelopes.push(tw.parseMapEnvelope(dFile.getItem(envInfo.start + e).data));
    }
  }
  var envPointsInfo = dFile.getType(tw.MAPITEMTYPE_ENVPOINTS);
  if (envPointsInfo != undefined && envPointsInfo.num > 0) {
    // Все точки лежат одним элементом; конверты ссылаются на них по индексу.
    this.envelopePoints = tw.parseEnvelopePoints(dFile.getItem(envPointsInfo.start).data, this.envelopes);
  }

  // map images
  var imgsInfo = dFile.getType(tw.MAPITEMTYPE_IMAGE);
  for (var i = 0; i < (imgsInfo ? imgsInfo.num : 0); i++) {
    var imgItem = dFile.getItem(imgsInfo.start + i);
    var imgInfo = tw.parseMapImage(imgItem.data);
    var imgName = tw.getCString(dFile.getData(imgInfo.imageName));

    if (imgInfo.external) {
      // load external texture
      var url = imgName + ".png";
      if (!/^https?:\/\//i.test(url)) {
        var baseUrl = tw.EXTERNAL_IMAGE_BASE_URL.replace(/\/+$/, "/");
        var normalizedUrl = url.replace(/^\/+/, "");
        url = baseUrl + normalizedUrl;
      }
      var extTex = tw.loadTexture({
        extern: true,
        imgUrl: url,
        name: imgName,
        imgId: i,
        width: imgInfo.width,
        height: imgInfo.height,
        format: imgInfo.format
      })

      if (extTex != undefined)
        this.textures.push(extTex);
    } else {

      var imgData = new Uint8Array(dFile.getData(imgInfo.imageData));
      var embTex = tw.loadTexture({
        extern: false,
        name: imgName,
        imgId: i,
        width: imgInfo.width,
        height: imgInfo.height,
        format: imgInfo.format,
        pixelBuf: imgData
      })

      if (embTex != undefined)
        this.textures.push(embTex);
    }
  }

  // groups, layers, tiles
  var groupsInfo = dFile.getType(tw.MAPITEMTYPE_GROUP);
  if (groupsInfo == undefined)
    return;

  var groupExType = tw.findExItemType(dFile, tw.GROUP_EX_UUID_INTS);

  for (var g = 0; g < groupsInfo.num; g++) {
    var groupItem = dFile.getItem(groupsInfo.start + g);
    var groupInfo = tw.parseMapGroup(groupItem.data);
    if (groupExType != undefined) {
      var groupExItem = dFile.findItem(groupExType, groupItem.id);
      if (groupExItem && groupExItem.data) {
        var groupEx = tw.parseMapGroupEx(groupExItem.data);
        if (groupEx)
          groupInfo.parallaxZoom = groupEx.parallaxZoom;
      }
    }

    var grp = new tw.Map.Group(this, groupInfo);
    this.groups.push(grp);

    var layersInfo = dFile.getType(tw.MAPITEMTYPE_LAYER);
    if (layersInfo == undefined)
      continue;

    for (var l = 0; l < groupInfo.numLayers; l++) {
      if (groupInfo.startLayer + l >= layersInfo.num)
        break;
      var layerItem = dFile.getItem(layersInfo.start + groupInfo.startLayer + l);
      var layerInfo = tw.parseMapLayer(layerItem.data);

      if (layerInfo.type == tw.LAYERTYPE_TILES) {
        var tileLayerInfo = tw.parseMapLayerTiles(layerItem.data);

        // Слой без картинки — это игровой слой (или слой сущностей). Из него
        // берём точку спавна: по ней удобно открывать карту, когда координата
        // ещё не задана. Сама камера здесь не двигается — решает вызывающая
        // страница (см. picker.js).
        if (tileLayerInfo.image == -1) {
          if (tileLayerInfo.flags & tw.TILESLAYERFLAG_GAME) {
            grp.gameLayer = true;
            var gameTiles = tw.parseGameplayIndices(dFile.getData(tileLayerInfo.data), tileLayerInfo.width * tileLayerInfo.height, tileLayerInfo.version);
            // Keep compact read-only indices for the editor's collision overlay.
            // No texture or GPU buffers are needed for this layer.
            if (!this.gameplay) {
              this.gameplay = { width: tileLayerInfo.width, height: tileLayerInfo.height,
                indices: gameTiles };
            }
            for (var sy = 0; sy < tileLayerInfo.height && !this.spawnPoint; sy++) {
              for (var sx = 0; sx < tileLayerInfo.width; sx++) {
                if (gameTiles[sy * tileLayerInfo.width + sx] >= tw.TILE_SPAWN && gameTiles[sy * tileLayerInfo.width + sx] <= tw.TILE_SPAWN + 2) {
                  this.spawnPoint = { x: sx * 32 + 16, y: sy * 32 + 16 };
                  break;
                }
              }
            }
          }
          continue;
        }

        var tileTex = this.getTex(tileLayerInfo.image);

        // PATCH (Editor Studio): раньше здесь была ссылка на несуществующую
        // quadLayerInfo — при пропавшей текстуре конструктор карты падал
        // целиком. Теперь просто пропускаем слой.
        if (tileTex == undefined) {
          console.warn("[map] нет текстуры для слоя тайлов:", tileLayerInfo.image);
        } else {
          // load tiles
          var tileData = dFile.getData(tileLayerInfo.data);
          var tiles = tw.parseLayerTiles(tileData, tileLayerInfo.width * tileLayerInfo.height, tileLayerInfo.version);
          // add layer
          grp.addTileLayer(tileLayerInfo.width, tileLayerInfo.height, tiles, tileLayerInfo.color, tileTex,
            tileLayerInfo.colorEnv, tileLayerInfo.colorEnvOffset);
        }

      } else if (layerInfo.type == tw.LAYERTYPE_QUADS) {
        var quadLayerInfo = tw.parseMapLayerQuads(layerItem.data);
        if (!quadLayerInfo.numQuads)
          continue;

        var quadData = dFile.getData(quadLayerInfo.data);
        if (quadData == undefined)
          continue;

        var quads = tw.parseLayerQuads(quadData, quadLayerInfo.numQuads);
        if (!quads.length)
          continue;

        // image == -1 — «без текстуры» (цветной прямоугольник, так задумано).
        // Явно сбрасываем quadTex на каждой итерации: var в ES5 всплывает
        // на уровень функции, и без сброса слои с image == -1 наследовали
        // текстуру предыдущего слоя (небо получало water, конусы фонарей — палитру).
        var quadTex = undefined;
        if (quadLayerInfo.image != -1) {
          quadTex = this.getTex(quadLayerInfo.image);
          if (quadTex == undefined || quadTex.failed)
            continue;
        }

        grp.addQuadLayer(quadTex, quads, layerInfo.flags);
      }
    }

  }
}

tw.Map.prototype.getTex = function (imgId) {
  for (var i = 0; i < this.textures.length; i++) {
    if (this.textures[i].imgId == imgId)
      return this.textures[i];
  }
}

tw.QuadLayer = function (texture, quads, flags) {
  this.type = tw.LAYERTYPE_QUADS;
  this.texture = texture;
  this.flags = flags || 0;
  this.quads = quads;
  this.built = false;

  // vertices buffer for all quads of this layer
  this.vBuf = new Float32Array(quads.length * 4 * 2);
  this.glVertexBuf = tw.gl.createBuffer();
  // vertex color buffer
  this.cBuf = new Float32Array(quads.length * 4 * 4);
  this.glColorBuf = tw.gl.createBuffer();
  // texture coords buffer
  this.tBuf = new Float32Array(quads.length * 4 * 2);
  this.glTexBuf = tw.gl.createBuffer();
  // index buffer
  this.iBuf = new Uint16Array(quads.length * 6);
  this.glIndexBuf = tw.gl.createBuffer();
}

/**
 * Буферы квадов собираются один раз: раньше tick() пересчитывал координаты,
 * цвета и заново заливал четыре буфера в GPU на каждом кадре — на больших
 * картах (сотни квадов) это была основная просадка FPS.
 */
tw.QuadLayer.prototype.tick = function () {
  if (this.built)
    return;
  this.built = true;

  var quads = this.quads;
  var color = [1, 1, 1, 1];
  var pos = [0, 0, 0, 0];
  var map = this.map || tw.map;
  var hasEnvelopes = map && map.envelopes && map.envelopes.length > 0;

  for (var i = 0; i < quads.length; i++) {
    var q = quads[i];

    // Смещение и поворот квада из конверта позиции (облака, туман).
    var offX = 0;
    var offY = 0;
    var rot = 0;
    if (hasEnvelopes && q.posEnv >= 0) {
      pos[0] = 0; pos[1] = 0; pos[2] = 0; pos[3] = 0;
      tw.evalEnvelope(map, q.posEnv, q.posEnvOffset, pos);
      offX = pos[0];
      offY = pos[1];
      rot = pos[2] / 180 * Math.PI;
    }

    var cx = q.points[4] ? q.points[4].x / 1024 : 0;
    var cy = q.points[4] ? q.points[4].y / 1024 : 0;
    var cos = Math.cos(rot);
    var sin = Math.sin(rot);

    var vertices = [];
    // Порядок вершин как в DDNet: 0, 2, 3, 1 (иначе картинка зеркалится).
    var order = [0, 2, 3, 1];
    for (var v = 0; v < 4; v++) {
      var point = q.points[order[v]];
      var px = point.x / 1024;
      var py = point.y / 1024;
      if (rot !== 0) {
        var dx = px - cx;
        var dy = py - cy;
        px = cx + dx * cos - dy * sin;
        py = cy + dx * sin + dy * cos;
      }
      vertices.push(px + offX, py + offY);
    }
    this.vBuf.set(vertices, i * 8);

    // Цвет вершины = цвет квада * цвет конверта (в игре это делает GPU).
    color[0] = 1; color[1] = 1; color[2] = 1; color[3] = 1;
    if (hasEnvelopes && q.colorEnv >= 0)
      tw.evalEnvelope(map, q.colorEnv, q.colorEnvOffset, color);

    var colors = [];
    for (var c = 0; c < 4; c++) {
      var vc = q.colors[order[c]];
      colors.push(
        Math.min(1, Math.max(0, vc.r / 255 * color[0])),
        Math.min(1, Math.max(0, vc.g / 255 * color[1])),
        Math.min(1, Math.max(0, vc.b / 255 * color[2])),
        Math.min(1, Math.max(0, vc.a / 255 * color[3]))
      );
    }
    this.cBuf.set(colors, i * 16);

    var coords = [];
    for (var t = 0; t < 4; t++) {
      var tc = q.texCoords[order[t]];
      coords.push(tc.x / 1024, tc.y / 1024);
    }
    this.tBuf.set(coords, i * 8);
  }

  // Границы слоя в координатах мира: по ним слой пропускается, если он
  // целиком за пределами вида (на больших картах это главный выигрыш).
  var minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
  for (var b = 0; b < this.vBuf.length; b += 2) {
    var bx = this.vBuf[b];
    var by = this.vBuf[b + 1];
    if (bx < minX) minX = bx;
    if (bx > maxX) maxX = bx;
    if (by < minY) minY = by;
    if (by > maxY) maxY = by;
  }
  this.bounds = quads.length && isFinite(minX)
    ? { x0: minX, y0: minY, x1: maxX, y1: maxY }
    : null;

  // Indices
  for (var k = 0; k < quads.length; k++) {
    var base = k * 4;
    this.iBuf.set([base, base + 1, base + 2, base, base + 2, base + 3], k * 6);
  }

  // Build gl buffers
  tw.gl.bindBuffer(tw.gl.ARRAY_BUFFER, this.glVertexBuf);
  tw.gl.bufferData(tw.gl.ARRAY_BUFFER, this.vBuf, tw.gl.STATIC_DRAW);

  tw.gl.bindBuffer(tw.gl.ARRAY_BUFFER, this.glColorBuf);
  tw.gl.bufferData(tw.gl.ARRAY_BUFFER, this.cBuf, tw.gl.STATIC_DRAW);

  tw.gl.bindBuffer(tw.gl.ARRAY_BUFFER, this.glTexBuf);
  tw.gl.bufferData(tw.gl.ARRAY_BUFFER, this.tBuf, tw.gl.STATIC_DRAW);

  tw.gl.bindBuffer(tw.gl.ELEMENT_ARRAY_BUFFER, this.glIndexBuf);
  tw.gl.bufferData(tw.gl.ELEMENT_ARRAY_BUFFER, this.iBuf, tw.gl.STATIC_DRAW);
}

tw.renderLine = function (x1, y1, x2, y2) {
  tw.gl.bindBuffer(tw.gl.ARRAY_BUFFER, tw.tmpBuf);
  tw.gl.bufferData(tw.gl.ARRAY_BUFFER, new Float32Array([x1, y1, x2, y2]), tw.gl.STATIC_DRAW);
  tw.gl.vertexAttribPointer(tw.stdShader.vPosAttr, 2, tw.gl.FLOAT, false, 0, 0);
  tw.gl.drawArrays(tw.gl.LINES, 0, 2);
}

tw.QuadLayer.prototype.render = function () {

  if (this.texture == undefined)
    tw.sTexCoords(0); //textureless quad
  else if (!this.texture.loaded)
    return; // texture not loaded
  else {
    tw.sTexCoords(1);
    tw.gl.bindTexture(tw.gl.TEXTURE_2D, this.texture.glTex);
  }

  // Enable vertex colors
  tw.sVertexColor(1);

  tw.setMatUniforms();

  tw.setColorMask([1.0, 1.0, 1.0, 1.0]);

  // Set attributes
  tw.gl.bindBuffer(tw.gl.ARRAY_BUFFER, this.glVertexBuf);
  tw.gl.vertexAttribPointer(tw.stdShader.vPosAttr, 2, tw.gl.FLOAT, false, 0, 0);

  tw.gl.bindBuffer(tw.gl.ARRAY_BUFFER, this.glColorBuf);
  tw.gl.vertexAttribPointer(tw.stdShader.vVertexColor, 4, tw.gl.FLOAT, false, 0, 0);

  tw.gl.bindBuffer(tw.gl.ARRAY_BUFFER, this.glTexBuf);
  tw.gl.vertexAttribPointer(tw.stdShader.vTexCoordAttr, 2, tw.gl.FLOAT, false, 0, 0);

  tw.gl.bindBuffer(tw.gl.ELEMENT_ARRAY_BUFFER, this.glIndexBuf);
  tw.gl.drawElements(tw.gl.TRIANGLES, this.quads.length * 6, tw.gl.UNSIGNED_SHORT, 0);

  // keep textures disabled by default
  tw.sTexCoords(0);
}

tw.Map.Group = function (map, info) {
  this.paraX = info.parallaxX;
  this.paraY = info.parallaxY;
  this.paraZoom = typeof info.parallaxZoom === "number" ? info.parallaxZoom : undefined;
  this.offsX = info.offsX;
  this.offsY = info.offsY;
  // Отсечение появилось во второй версии группы (см. CMapItemGroup).
  this.version = info.version == null ? 0 : info.version;
  // В группе с игровым слоем движок отключает отсечение (CLayers::Init) —
  // иначе карта могла бы оказаться обрезанной целиком.
  this.gameLayer = false;
  this.useClipping = !!info.useClipping;
  this.clipX = info.clipX || 0;
  this.clipY = info.clipY || 0;
  this.clipW = info.clipW || 0;
  this.clipH = info.clipH || 0;
  this.map = map;
  this.layers = [];
}

tw.Map.Group.prototype.addQuadLayer = function (texture, quads, flags) {
  var layer = new tw.QuadLayer(texture, quads, flags);
  layer.map = this.map;
  this.layers.push(layer);
}

tw.Map.Group.prototype.addTileLayer = function (width, height, tiles, color, texture, colorEnv, colorEnvOffset) {
  var newLayer = new tw.TileLayer(width, height, tiles, color, this);
  newLayer.map = this.map;
  newLayer.texture = texture;
  newLayer.colorEnv = colorEnv == null ? -1 : colorEnv;
  newLayer.colorEnvOffset = colorEnvOffset || 0;
  this.layers.push(newLayer);
}

tw.Map.Group.prototype.tick = function () {
  for (var i = 0; i < this.layers.length; i++)
    if (this.layers[i].editorHidden !== true) this.layers[i].tick();
}

/**
 * Базовый размер экрана DDNet (CRenderTools::CalcScreenParams, Zoom = 1.0):
 * Amount = 1150 * 1000, WMax = 1500, HMax = 1050. Полноэкранные фоновые квады
 * на картах DDNet рисуются под эти пределы (около ±750 × ±525).
 */
tw.calcScreenParams = function (aspect) {
  var a = (typeof aspect === "number" && isFinite(aspect) && aspect > 0) ? aspect : 1;
  var amount = 1150 * 1000;
  var wMax = 1500;
  var hMax = 1050;
  var f = Math.sqrt(amount) / Math.sqrt(a);
  var w = f * a;
  var h = f;
  if (w > wMax) {
    w = wMax;
    h = w / a;
  }
  if (h > hMax) {
    h = hMax;
    w = h * a;
  }
  return { w: w, h: h };
}

/**
 * Экранный прямоугольник группы — формула DDNet (CRenderTools::MapScreenToGroup):
 *   scale = (parallaxZoom * (invDdnetZoom - 1) + 100) / 100
 * Для parallaxZoom === 100 прямоугольник в точности равен игровому виду
 * (tw.worldView * aspect / zoom × tw.worldView / zoom), а для фоновых слоёв
 * (parallaxZoom < 100) опирается на базовое окно CalcScreenParams без
 * ложного «раздувания» фрустума при zoom < 1.
 */
tw.Map.Group.prototype.initMapScreen = function () {
  var zoom = (typeof tw.cameraZoom === "number" && isFinite(tw.cameraZoom) && tw.cameraZoom > 0) ? tw.cameraZoom : 1;
  var aspect = (typeof tw.aspect === "number" && isFinite(tw.aspect) && tw.aspect > 0) ? tw.aspect : 1;

  var worldWidth = (tw.worldView[0] * aspect) / zoom;
  var worldHeight = tw.worldView[1] / zoom;

  var parallaxZoom = typeof this.paraZoom === "number"
    ? this.paraZoom
    : Math.max(this.paraX, this.paraY);
  if (parallaxZoom < 0) parallaxZoom = 0;
  if (parallaxZoom > 100) parallaxZoom = 100;

  var width, height;
  if (parallaxZoom >= 100) {
    width = worldWidth;
    height = worldHeight;
  } else {
    var ddBase = tw.calcScreenParams(aspect);
    var scaleX = (parallaxZoom * (worldWidth / ddBase.w - 1) + 100) / 100;
    var scaleY = (parallaxZoom * (worldHeight / ddBase.h - 1) + 100) / 100;
    if (!isFinite(scaleX) || scaleX <= 0)
      scaleX = worldWidth / ddBase.w;
    if (!isFinite(scaleY) || scaleY <= 0)
      scaleY = worldHeight / ddBase.h;
    width = ddBase.w * scaleX;
    height = ddBase.h * scaleY;
  }

  var cx = tw.cameraPos[0] * (this.paraX / 100);
  var cy = tw.cameraPos[1] * (this.paraY / 100);

  var p1 = this.offsX + cx - width / 2;
  var p2 = this.offsY + cy - height / 2;
  var p3 = p1 + width;
  var p4 = p2 + height;

  tw.setMapScreen(p1, p2, p3, p4);
}

tw.initPrjMat = function () {
  // Init projection matrix
  tw.mat4.ortho(tw.prjMat, tw.mapScreen[0], tw.mapScreen[2], tw.mapScreen[3], tw.mapScreen[1], 1, -1);
  tw.setMatUniforms();
}

/**
 * Экранный прямоугольник камеры в координатах мира — «интерфейс» игры
 * (IGraphics::MapScreenToInterface): параллакс сюда не входит, поэтому
 * координаты прямоугольника отсечения группы читаются в тех же единицах,
 * что и координаты точек на карте.
 */
tw.interfaceRect = function () {
  var zoom = tw.cameraZoom > 0 ? tw.cameraZoom : 1;
  var w = (tw.worldView[0] * (tw.aspect || 1)) / zoom;
  var h = tw.worldView[1] / zoom;
  return {
    x0: tw.cameraPos[0] - w / 2,
    y0: tw.cameraPos[1] - h / 2,
    x1: tw.cameraPos[0] + w / 2,
    y1: tw.cameraPos[1] + h / 2,
    w: w,
    h: h
  };
}

/** Пересекаются ли два прямоугольника (с запасом margin в пикселях мира). */
tw.rectsOverlap = function (a, b, margin) {
  if (!a || !b) return true;
  var m = margin || 0;
  return a.x1 >= b.x0 - m && a.x0 <= b.x1 + m && a.y1 >= b.y0 - m && a.y0 <= b.y1 + m;
}

/** Видимый прямоугольник группы в координатах мира (её mapScreen). */
tw.Map.Group.prototype.worldRect = function () {
  var ms = tw.mapScreen;
  return {
    x0: Math.min(ms[0], ms[2]),
    y0: Math.min(ms[1], ms[3]),
    x1: Math.max(ms[0], ms[2]),
    y1: Math.max(ms[1], ms[3]),
    w: Math.abs(ms[2] - ms[0]),
    h: Math.abs(ms[3] - ms[1])
  };
}

/**
 * Область отсечения группы (m_UseClipping) — как в движке
 * (CRenderLayerGroup::DoRender): прямоугольник m_Clip* задан в мировых
 * координатах и переводится в пиксели окна по обычной камере, без параллакса
 * группы. Всё, что группа рисует за пределами прямоугольника, не видно.
 *
 * Возвращает false, если область целиком за экраном: такую группу можно
 * вообще не рисовать.
 */
tw.Map.Group.prototype.applyClip = function () {
  var gl = tw.gl;

  var clipping = this.useClipping && this.version >= 2 && !this.gameLayer;
  if (!clipping) {
    gl.disable(gl.SCISSOR_TEST);
    return true;
  }

  // A clipped group with an empty/invalid rectangle has no visible area.
  // Disabling scissor here would accidentally render every quad in the group.
  if (this.clipW <= 0 || this.clipH <= 0) {
    gl.disable(gl.SCISSOR_TEST);
    return false;
  }

  var view = tw.interfaceRect();
  var screenW = tw.canvas ? tw.canvas.width : 0;
  var screenH = tw.canvas ? tw.canvas.height : 0;
  if (!screenW || !screenH || view.w <= 0 || view.h <= 0) {
    gl.disable(gl.SCISSOR_TEST);
    return true;
  }

  // Прямоугольник отсечения в пикселях окна.
  var scaleX = screenW / view.w;
  var scaleY = screenH / view.h;
  var left = (this.clipX - view.x0) * scaleX;
  var top = (this.clipY - view.y0) * scaleY;
  var right = (this.clipX + this.clipW - view.x0) * scaleX;
  var bottom = (this.clipY + this.clipH - view.y0) * scaleY;

  if (right < 0 || left > screenW || bottom < 0 || top > screenH) {
    gl.disable(gl.SCISSOR_TEST);
    return false;
  }

  var x0 = Math.max(0, Math.floor(Math.min(left, right)));
  var y0 = Math.max(0, Math.floor(Math.min(top, bottom)));
  var x1 = Math.min(screenW, Math.ceil(Math.max(left, right)));
  var y1 = Math.min(screenH, Math.ceil(Math.max(top, bottom)));
  var w = x1 - x0;
  var h = y1 - y0;

  if (w <= 0 || h <= 0) {
    gl.disable(gl.SCISSOR_TEST);
    return false;
  }

  gl.enable(gl.SCISSOR_TEST);
  // WebGL считает Y снизу, карта — сверху.
  gl.scissor(x0, screenH - y1, w, h);
  return true;
}

tw.Map.Group.prototype.render = function () {
  var gl = tw.gl;

  this.initMapScreen();
  tw.initPrjMat();

  if (!this.applyClip())
    return;

  // Слои, которые целиком за пределами вида группы, не рисуем: на больших
  // картах это убирает основную часть работы при приближении.
  var rect = this.worldRect();
  var margin = Math.max(32, rect.h * 0.02);

  try {
    for (var i = 0; i < this.layers.length; i++) {
      var layer = this.layers[i];
      if (layer.editorHidden === true) continue;
      if (layer.bounds && !tw.rectsOverlap(layer.bounds, rect, margin))
        continue;
      layer.render();
    }
  } finally {
    // Never let one group leak its scissor into the next group/frame.
    gl.disable(gl.SCISSOR_TEST);
  }
}

tw.Map.prototype.tick = function () {
  for (var i = 0; i < this.groups.length; i++)
    this.groups[i].tick();
}

// test function
tw.buildImage = function (width, height, buf) {
  var cnvs = document.createElement("canvas");

  var ctx = cnvs.getContext("2d");



  var imgData = ctx.createImageData(width, height);


  var len = width * height;
  for (var i = 0; i < len * 4; i++) {
    imgData.data[i] = buf[i];
  }

  cnvs.width = width;
  cnvs.height = height;


  ctx.putImageData(imgData, 0, 0);

  // export as png
  var img = new Image();
  img.src = cnvs.toDataURL();
  return img;
}

tw.Map.prototype.render = function () {
  // Render all groups
  for (var i = 0; i < this.groups.length; i++)
    this.groups[i].render();

  // Unbind texture
  tw.gl.bindTexture(tw.gl.TEXTURE_2D, null);
}

tw.isPowerOfTwo = function (x) {
  return Number.isInteger(x) && x > 0 && (x & (x - 1)) === 0;
}

tw.nextPowerOfTwo = function (x) {
  var v = Math.max(1, Math.floor(x || 1));
  var p = 1;
  while (p < v && p < 0x40000000)
    p <<= 1;
  return p;
}

/**
 * Билинейное масштабирование RGBA-буфера к размерам 2^n для WebGL 1, где
 * текстуры не кратные степени двойки иначе запрещают gl.REPEAT и мипмапы
 * (из-за CLAMP_TO_EDGE повторяющиеся фоновые квады превращались в полосы).
 * Интерполяция взвешена по альфе, чтобы прозрачные пиксели не давали кайму.
 */
tw.resampleRgba = function (src, srcW, srcH, dstW, dstH) {
  if (srcW === dstW && srcH === dstH)
    return src;
  var out = new Uint8Array(dstW * dstH * 4);
  var scaleX = srcW / dstW;
  var scaleY = srcH / dstH;
  for (var y = 0; y < dstH; y++) {
    var sy = (y + 0.5) * scaleY - 0.5;
    var y0 = Math.floor(sy);
    var fy = sy - y0;
    var y1 = y0 + 1;
    // Повторяем края по модулю размера (текстуры квадов зациклены).
    var wy0 = ((y0 % srcH) + srcH) % srcH;
    var wy1 = ((y1 % srcH) + srcH) % srcH;
    for (var x = 0; x < dstW; x++) {
      var sx = (x + 0.5) * scaleX - 0.5;
      var x0 = Math.floor(sx);
      var fx = sx - x0;
      var x1 = x0 + 1;
      var wx0 = ((x0 % srcW) + srcW) % srcW;
      var wx1 = ((x1 % srcW) + srcW) % srcW;

      var i00 = (wy0 * srcW + wx0) * 4;
      var i10 = (wy0 * srcW + wx1) * 4;
      var i01 = (wy1 * srcW + wx0) * 4;
      var i11 = (wy1 * srcW + wx1) * 4;

      var w00 = (1 - fx) * (1 - fy);
      var w10 = fx * (1 - fy);
      var w01 = (1 - fx) * fy;
      var w11 = fx * fy;

      var a00 = src[i00 + 3], a10 = src[i10 + 3], a01 = src[i01 + 3], a11 = src[i11 + 3];
      var a = w00 * a00 + w10 * a10 + w01 * a01 + w11 * a11;
      var dstOff = (y * dstW + x) * 4;
      if (a > 0.001) {
        var r = (w00 * src[i00] * a00 + w10 * src[i10] * a10 + w01 * src[i01] * a01 + w11 * src[i11] * a11) / a;
        var g = (w00 * src[i00 + 1] * a00 + w10 * src[i10 + 1] * a10 + w01 * src[i01 + 1] * a01 + w11 * src[i11 + 1] * a11) / a;
        var b = (w00 * src[i00 + 2] * a00 + w10 * src[i10 + 2] * a10 + w01 * src[i01 + 2] * a01 + w11 * src[i11 + 2] * a11) / a;
        out[dstOff] = Math.round(r);
        out[dstOff + 1] = Math.round(g);
        out[dstOff + 2] = Math.round(b);
        out[dstOff + 3] = Math.round(a);
      }
    }
  }
  return out;
}

tw.rgbToRgba = function (rgb, width, height) {
  var total = width * height;
  var out = new Uint8Array(total * 4);
  for (var i = 0; i < total; i++) {
    out[i * 4] = rgb[i * 3];
    out[i * 4 + 1] = rgb[i * 3 + 1];
    out[i * 4 + 2] = rgb[i * 3 + 2];
    out[i * 4 + 3] = 255;
  }
  return out;
}

tw.textureFailures = [];

tw.loadTexture = function (attrs) {
  var gl = tw.gl;

  var tex = {
    name: attrs.name,
    imgId: attrs.imgId,
    width: attrs.width,
    height: attrs.height,
    loaded: false,
    failed: false
  }

  var isWebGL2 = !!tw.isWebGL2 ||
    (typeof WebGL2RenderingContext !== "undefined" && gl instanceof WebGL2RenderingContext) ||
    (gl && typeof gl.texStorage2D === "function");
  var maxTexSize = (gl && typeof gl.getParameter === "function" && gl.MAX_TEXTURE_SIZE)
    ? (gl.getParameter(gl.MAX_TEXTURE_SIZE) || 4096)
    : 4096;

  var applyRepeatAndMipmaps = function () {
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.REPEAT);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_NEAREST);
    gl.generateMipmap(gl.TEXTURE_2D);
  };

  tex.glTex = tw.gl.createTexture();

  if (attrs.extern) {
    // load texture from image url
    tex.image = new Image();
    // PATCH (Editor Studio): текстуры могут прийти с map-api другого домена —
    // без crossOrigin такой Image нельзя отдать в WebGL (texImage2D бросит
    // SecurityError). Для своего домена атрибут не нужен и только мешает кешу.
    try {
      var target = new URL(attrs.imgUrl, window.location.href);
      if (target.origin !== window.location.origin)
        tex.image.crossOrigin = "anonymous";
    } catch (err) { /* относительный адрес — тот же домен */ }
    tex.image.src = attrs.imgUrl;

    tex.image.onload = function (e) {
      var imgW = tex.image.naturalWidth || tex.image.width || attrs.width;
      var imgH = tex.image.naturalHeight || tex.image.height || attrs.height;
      var pot = tw.isPowerOfTwo(imgW) && tw.isPowerOfTwo(imgH);
      var source = tex.image;

      if (!pot && !isWebGL2 && typeof document !== "undefined" && typeof document.createElement === "function") {
        var potW = Math.min(maxTexSize, tw.nextPowerOfTwo(imgW));
        var potH = Math.min(maxTexSize, tw.nextPowerOfTwo(imgH));
        var offscreen = document.createElement("canvas");
        offscreen.width = potW;
        offscreen.height = potH;
        var ctx2d = offscreen.getContext && offscreen.getContext("2d");
        if (ctx2d) {
          ctx2d.drawImage(tex.image, 0, 0, potW, potH);
          source = offscreen;
          pot = true;
        }
      }

      gl.bindTexture(gl.TEXTURE_2D, tex.glTex);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, source);
      if (pot || isWebGL2) {
        applyRepeatAndMipmaps();
      } else {
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      }

      gl.bindTexture(gl.TEXTURE_2D, null);

      tex.loaded = true;
      tw.requestRender(3);
    }

    // PATCH (Editor Studio): раньше ошибка только писалась в консоль, и
    // слой с «пустой» текстурой рисовался белым прямоугольником. Теперь
    // текстура помечается как неудачная, такие слои пропускаются, а список
    // пропавших файлов можно показать пользователю (tw.missingTextures).
    tex.image.onerror = function (e) {
      tex.failed = true;
      tex.loaded = false;
      tw.textureFailures.push({ name: attrs.name, url: attrs.imgUrl });
      console.warn("[map] текстура не загрузилась:", attrs.name, attrs.imgUrl);
    }
  } else {
    // load texture from pixelbuffer
    var srcBuf = attrs.pixelBuf;
    var expectedRgba = attrs.width * attrs.height * 4;
    var expectedRgb = attrs.width * attrs.height * 3;
    if (srcBuf && srcBuf.length === expectedRgb && expectedRgb !== expectedRgba) {
      srcBuf = tw.rgbToRgba(srcBuf, attrs.width, attrs.height);
    }
    if (!srcBuf || srcBuf.length !== expectedRgba) {
      tex.failed = true;
      console.warn("[map] неверный размер встроенной текстуры:", attrs.name, srcBuf ? srcBuf.length : 0);
    } else {
      var uploadW = attrs.width;
      var uploadH = attrs.height;
      var uploadBuf = srcBuf;
      var potBuf = tw.isPowerOfTwo(uploadW) && tw.isPowerOfTwo(uploadH);

      if (!potBuf && !isWebGL2) {
        var dstW = Math.min(maxTexSize, tw.nextPowerOfTwo(uploadW));
        var dstH = Math.min(maxTexSize, tw.nextPowerOfTwo(uploadH));
        uploadBuf = tw.resampleRgba(srcBuf, uploadW, uploadH, dstW, dstH);
        uploadW = dstW;
        uploadH = dstH;
        potBuf = true;
      }

      gl.bindTexture(gl.TEXTURE_2D, tex.glTex);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, uploadW, uploadH, 0, gl.RGBA, gl.UNSIGNED_BYTE, uploadBuf);
      if (potBuf || isWebGL2) {
        applyRepeatAndMipmaps();
      } else {
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      }

      gl.bindTexture(gl.TEXTURE_2D, null);

      tex.loaded = true;
      tw.requestRender(3);
    }
  }

  return tex;
}

window.requestAnimFrame = (function () {
  if (window.requestAnimationFrame)
    return window.requestAnimationFrame;
  if (window.webkit && window.webkit.RequestAnimationFrame)
    return window.webkit.RequestAnimationFrame;
  if (window.mozRequestAnimationFrame)
    return window.mozRequestAnimationFrame;

  return function (cb) {
    window.setTimeout(cb, 1000 / 60);
  };
})();

tw.buildShader = function (str, type) {
  var shader = tw.gl.createShader(type);
  tw.gl.shaderSource(shader, str);
  tw.gl.compileShader(shader);

  if (!tw.gl.getShaderParameter(shader, tw.gl.COMPILE_STATUS)) {
    console.log(tw.gl.getShaderInfoLog(shader));
    return null;
  }

  return shader;
}

tw.loopStarted = false;
tw.loopScheduled = false;

tw.scheduleMainLoop = function () {
  if (!tw.loopStarted || tw.loopScheduled)
    return;
  tw.loopScheduled = true;
  requestAnimFrame(function () {
    tw.loopScheduled = false;
    tw.mainLoop();
  });
}

tw.mainLoop = function () {
  tw.loopStarted = true;
  var now = Date.now();
  var dt = now - tw.lastMainLoopTs;
  if (dt < 0 || dt > 250)
    dt = 16;
  tw.lastMainLoopTs = now;

  var prevZoom = tw.cameraZoom;

  tw.cameraZoom = tw.cameraZoomTarget;

  if (prevZoom !== tw.cameraZoom) {
    var widthPrev = tw.worldView[0] * tw.aspect / prevZoom;
    var widthNow = tw.worldView[0] * tw.aspect / tw.cameraZoom;
    var heightPrev = tw.worldView[1] / prevZoom;
    var heightNow = tw.worldView[1] / tw.cameraZoom;

    var zoomOffsetX = (tw.zoomAnchor[0] - 0.5) * (widthPrev - widthNow);
    var zoomOffsetY = (tw.zoomAnchor[1] - 0.5) * (heightPrev - heightNow);
    tw.cameraPos[0] += zoomOffsetX;
    tw.cameraPos[1] += zoomOffsetY;
    tw.cameraPosTarget[0] += zoomOffsetX;
    tw.cameraPosTarget[1] += zoomOffsetY;
  }

  // Transform mouse coords to world coords
  // Дельты приходят в CSS-пикселях: считаем по размеру окна, а не канвы,
  // иначе на экранах с плотностью 2 карта уезжает вдвое медленнее курсора.
  var inputWidth = tw.cssWidth || tw.canvas.width;
  var inputHeight = tw.cssHeight || tw.canvas.height;
  tw.mouseDownInc[0] = tw.mouseDownInc[0] / inputWidth * tw.worldView[0] * tw.aspect / tw.cameraZoom;
  tw.mouseDownInc[1] = tw.mouseDownInc[1] / inputHeight * tw.worldView[1] / tw.cameraZoom;

  var move = tw.getKeyboardMove();
  if (move[0] !== 0 || move[1] !== 0)
    tw.keyboardHoldMs += dt;
  else
    tw.keyboardHoldMs = 0;

  var keyboardSpeed = tw.getKeyboardSpeed();
  var inputMoveX = tw.mouseDownInc[0] + move[0] * (keyboardSpeed / tw.cameraZoom);
  var inputMoveY = tw.mouseDownInc[1] + move[1] * (keyboardSpeed / tw.cameraZoom);

  if (inputMoveX !== 0 || inputMoveY !== 0) {
    tw.moveVelocity[0] += (inputMoveX - tw.moveVelocity[0]) * tw.MOVE_ACCELERATION;
    tw.moveVelocity[1] += (inputMoveY - tw.moveVelocity[1]) * tw.MOVE_ACCELERATION;
  } else {
    tw.moveVelocity[0] *= tw.MOVE_DECELERATION;
    tw.moveVelocity[1] *= tw.MOVE_DECELERATION;
    if (Math.abs(tw.moveVelocity[0]) < 0.0001)
      tw.moveVelocity[0] = 0;
    if (Math.abs(tw.moveVelocity[1]) < 0.0001)
      tw.moveVelocity[1] = 0;
  }

  tw.cameraPosTarget[0] += tw.moveVelocity[0];
  tw.cameraPosTarget[1] += tw.moveVelocity[1];

  var posDeltaX = tw.cameraPosTarget[0] - tw.cameraPos[0];
  var posDeltaY = tw.cameraPosTarget[1] - tw.cameraPos[1];
  tw.cameraPos[0] += posDeltaX * tw.MOVE_SMOOTHING;
  tw.cameraPos[1] += posDeltaY * tw.MOVE_SMOOTHING;

  // Догоняем цель за конечное время: иначе камера «ползёт» бесконечно и
  // кадры рисуются вечно (на слабых машинах — постоянная нагрузка CPU/GPU).
  if (Math.abs(tw.cameraPosTarget[0] - tw.cameraPos[0]) < 0.05)
    tw.cameraPos[0] = tw.cameraPosTarget[0];
  if (Math.abs(tw.cameraPosTarget[1] - tw.cameraPos[1]) < 0.05)
    tw.cameraPos[1] = tw.cameraPosTarget[1];

  var moving =
    tw.cameraPos[0] !== tw.cameraPosTarget[0] ||
    tw.cameraPos[1] !== tw.cameraPosTarget[1] ||
    tw.moveVelocity[0] !== 0 ||
    tw.moveVelocity[1] !== 0;

  tw.mouseDownInc[0] = 0;
  tw.mouseDownInc[1] = 0;

  // PATCH (Editor Studio): карта рисуется по требованию. Простой открытый
  // просмотрщик больше не жжёт батарею и не грузит GPU: кадр появляется при
  // движении камеры, изменении размера окна или загрузке текстур.
  var shouldRender = tw.autoRender || tw.dirty || moving || tw.renderFrames > 0;
  if (shouldRender) {
    tw.render();
    tw.dirty = false;
    if (!moving && tw.renderFrames > 0)
      tw.renderFrames--;
  }

  // Reset zoomed state
  tw.zoomed = false;


  var keyMove = tw.getKeyboardMove();
  if (tw.autoRender || moving || tw.renderFrames > 0 || tw.mousePressed || keyMove[0] !== 0 || keyMove[1] !== 0)
    tw.scheduleMainLoop();
}

/**
 * Запросить кадр (после программного изменения камеры, загрузки текстуры…).
 * frames — сколько кадров подряд нарисовать: текстуры приходят по одной.
 */
tw.requestRender = function (frames) {
  tw.dirty = true;
  var n = typeof frames === 'number' ? frames : 2;
  if (n > (tw.renderFrames || 0))
    tw.renderFrames = n;
  tw.scheduleMainLoop();
};

tw.setMatUniforms = function () {
  tw.gl.uniformMatrix4fv(tw.stdShader.pMatUni, false, tw.prjMat);
  tw.gl.uniformMatrix4fv(tw.stdShader.mvMatUni, false, tw.mvMat);
}

/**
 * Размер канвы. Раньше width/height присваивались каждый кадр — а это сброс
 * буфера и повторное выделение памяти даже при тех же числах. Теперь только
 * при реальном изменении, и с учётом плотности пикселей экрана (карта
 * остаётся резкой на retina, но не больше maxPixelRatio).
 * Возвращает true, если размер изменился (нужен кадр).
 */
tw.screenResize = function () {
  var canvas = tw.canvas;
  if (!canvas)
    return false;

  var cssWidth = Math.max(1, Math.round(canvas.clientWidth || window.innerWidth));
  var cssHeight = Math.max(1, Math.round(canvas.clientHeight || window.innerHeight));

  var ratio = window.devicePixelRatio || 1;
  var maxRatio = typeof tw.config.maxPixelRatio === 'number' ? tw.config.maxPixelRatio : 2;
  if (ratio > maxRatio) ratio = maxRatio;
  if (ratio < 1) ratio = 1;

  var width = Math.max(1, Math.round(cssWidth * ratio));
  var height = Math.max(1, Math.round(cssHeight * ratio));

  tw.cssWidth = cssWidth;
  tw.cssHeight = cssHeight;
  tw.pixelRatio = ratio;
  tw.aspect = cssWidth / cssHeight;

  if (canvas.width === width && canvas.height === height)
    return false;

  canvas.width = width;
  canvas.height = height;
  return true;
}

tw.render = function () {
  var gl = tw.gl;

  if (tw.screenResize())
    tw.renderFrames = Math.max(tw.renderFrames || 0, 1);

  // Set viewport
  tw.gl.viewportWidth = tw.canvas.width;
  tw.gl.viewportHeight = tw.canvas.height;

  gl.viewport(0, 0, gl.viewportWidth, gl.viewportHeight);
  gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

  // reset matrices
  tw.mat4.identity(tw.mvMat);
  tw.mat4.identity(tw.prjMat);

  // enable texture 0
  gl.activeTexture(gl.TEXTURE0);
  gl.uniform1i(tw.stdShader.uSampler, 0);

  this.map.tick();
  this.map.render();

  // PATCH (Editor Studio): оверлей (метка точки) рисуется в том же кадре,
  // поэтому метка не отстаёт от карты при перетаскивании.
  if (typeof tw.onAfterRender === "function")
    tw.onAfterRender();
}

tw.TileLayer = function (width, height, tiles, color, group) {
  this.type = tw.LAYERTYPE_TILES;
  this.width = width;
  this.height = height;
  this.tiles = tiles;
  this.numTiles = this.renderTileNum();

  this.baseColor = color;
  this.color = new Float32Array([color[0] / 255, color[1] / 255, color[2] / 255, color[3] / 255]);
  this.colorEnv = -1;
  this.colorEnvOffset = 0;
  this.builtViewKey = '';
  this.vertexFloatArray = new Float32Array(this.numTiles * 12);
  this.texCoordFloatArray = new Float32Array(this.numTiles * 12);
  this.vertexBuf = undefined;
  this.texCoordBuf = undefined;
  this.tileSize = 32;
  this.needInit = true;
  this.group = group;
}

// get the number of tiles that will be rendered
tw.TileLayer.prototype.renderTileNum = function () {
  var num = 0;
  for (var i = 0; i < this.tiles.length; i++) {
    if (this.tiles[i].index != 0)
      num++;
  }

  return num;
}

/**
 * Пересборка тайлов нужна только при смене масштаба или размера окна:
 * шаг текстуры для мипмапов зависит от них. Раньше это происходило лишь по
 * событию колеса мыши, поэтому кнопки «Вся карта» и «К точке» давали
 * «плывущие» края тайлов.
 */
tw.TileLayer.prototype.viewKey = function () {
  var w = tw.canvas ? tw.canvas.width : 0;
  var h = tw.canvas ? tw.canvas.height : 0;
  return (tw.cameraZoom || 1).toFixed(6) + '|' + w + 'x' + h;
}

tw.TileLayer.prototype.tick = function () {
  if (!this.bounds) {
    this.bounds = {
      x0: 0,
      y0: 0,
      x1: this.width * 32,
      y1: this.height * 32
    };
  }
  var key = this.viewKey();
  if (this.needInit || this.builtViewKey !== key) {
    this.needInit = false;
    this.builtViewKey = key;
    // Re / initialise buffers
    this.group.initMapScreen();
    this.initBuffers();
    this.updateColor();
  }
}

/** Цвет тайлов = цвет слоя * цвет конверта (день/ночь). */
tw.TileLayer.prototype.updateColor = function () {
  var env = [1, 1, 1, 1];
  var map = this.map || tw.map;
  if (map && map.envelopes && this.colorEnv >= 0)
    tw.evalEnvelope(map, this.colorEnv, this.colorEnvOffset, env);

  for (var i = 0; i < 4; i++) {
    var v = (this.baseColor[i] / 255) * env[i];
    this.color[i] = v < 0 ? 0 : (v > 1 ? 1 : v);
  }
}

tw.TileLayer.prototype.setTileXY = function (x, y, index, flags) {
  var t = this.tiles[y * this.width + x];

  t.index = index;
  t.flags = flags;
}

tw.TileLayer.prototype.setTile = function (i, index, flags) {
  var t = this.tiles[i];

  t.index = index;
  t.flags = flags;
}

tw.setArray = function (dstArray, offset, newArray) {
  for (var i = 0; i < newArray.length; i++) {
    dstArray[offset + i] = newArray[i];
  }
}

// Build vertices and init gl buffers
tw.TileLayer.prototype.initBuffers = function () {
  var x, y, t;

  // mipmap border correction
  var tilePixelSize = 1024 / 32;
  var finalTileSize = 32 / (tw.mapScreen[2] - tw.mapScreen[0]) * tw.canvas.width;
  var finalTilesetScale = finalTileSize / tilePixelSize

  var texSize = 1024.0
  var frac = (1.25 / texSize) * (1 / finalTilesetScale)
  var nudge = (0.5 / texSize) * (1 / finalTilesetScale)

  //
  var vertices;
  var indices;
  var texCoords;

  t = 0;

  // Vertices and texture coordinates
  for (y = 0; y < this.height; y++) {
    for (x = 0; x < this.width; x++) {
      var curTile = this.tiles[y * this.width + x];
      var index = curTile.index;

      // skip tiles with index 0
      if (index == 0)
        continue;

      vertices = [
        x, y,
        x, y + 1.0,
        x + 1.0, y + 1.0,
        x, y,
        x + 1.0, y + 1.0,
        x + 1.0, y
      ];

      this.vertexFloatArray.set(vertices, t * 12);

      // Get subset offsets
      //TODO: fix border artefacts
      var tx = index % 16;
      var ty = Math.floor(index / 16);

      var px0 = tx * (1024 / 16);
      var py0 = ty * (1024 / 16);
      var px1 = px0 + (1024 / 16) - 1;
      var py1 = py0 + (1024 / 16) - 1;

      var tmp;
      var x0, x1, x2, x3;
      var y0, y1, y2, y3;

      x0 = nudge + px0 / 1024 + frac;
      x1 = nudge + px1 / 1024 - frac;
      x2 = x1
      x3 = x0

      y0 = nudge + py0 / 1024 + frac;
      y1 = nudge + py1 / 1024 - frac;
      y2 = y1;
      y3 = y0;

      // Handle tile flags
      if (curTile.flags & tw.TILEFLAG_HFLIP) {
        y0 = y2;
        y2 = y3;
        y3 = y0;
        y1 = y2;

      }

      if (curTile.flags & tw.TILEFLAG_VFLIP) {
        x0 = x1;
        x2 = x3;
        x3 = x0;
        x1 = x2;
      }

      if (curTile.flags & tw.TILEFLAG_ROTATE) {
        tmp = y0;
        y0 = y1;
        y1 = y2;
        y2 = y3;
        y3 = tmp;

        tmp = x0;
        x0 = x3;
        x3 = x1;
        x1 = x2;
        x2 = tmp;
      }

      texCoords = [
        x0, y0,
        x3, y1,
        x1, y2,
        x0, y0,
        x1, y2,
        x2, y3
      ]

      this.texCoordFloatArray.set(texCoords, t * 12)

      t++;
    }
  }


  // Init gl buffers
  if (this.vertexBuf == undefined) {
    this.vertexBuf = tw.gl.createBuffer();
    this.texCoordBuf = tw.gl.createBuffer();
  }

  tw.gl.bindBuffer(tw.gl.ARRAY_BUFFER, this.vertexBuf);
  tw.gl.bufferData(tw.gl.ARRAY_BUFFER, this.vertexFloatArray, tw.gl.STATIC_DRAW);

  tw.gl.bindBuffer(tw.gl.ARRAY_BUFFER, this.texCoordBuf);
  tw.gl.bufferData(tw.gl.ARRAY_BUFFER, this.texCoordFloatArray, tw.gl.STATIC_DRAW);
}

tw.TileLayer.prototype.render = function () {

  if (!this.texture.loaded)
    return; // we can't render without texture

  // Enable texture
  tw.sTexCoords(1);
  tw.gl.bindTexture(tw.gl.TEXTURE_2D, this.texture.glTex);

  // Vertex colors are not needed
  tw.sVertexColor(0);

  // MapTile resize
  tw.mat4.copy(tw.tmpMat, tw.mvMat);
  tw.mat4.scale(tw.mvMat, tw.mvMat, [32, 32, 0.0]);
  tw.setMatUniforms();

  // Set color mask (цвет слоя с учётом конверта — см. updateColor)
  tw.setColorMask(this.color);

  // Vertex attribute
  tw.gl.bindBuffer(tw.gl.ARRAY_BUFFER, this.vertexBuf);
  tw.gl.vertexAttribPointer(tw.stdShader.vPosAttr, 2, tw.gl.FLOAT, false, 0, 0);
  // Texture coord attribute
  tw.gl.bindBuffer(tw.gl.ARRAY_BUFFER, this.texCoordBuf);
  tw.gl.vertexAttribPointer(tw.stdShader.vTexCoordAttr, 2, tw.gl.FLOAT, false, 0, 0);
  tw.gl.drawArrays(tw.gl.TRIANGLES, 0, this.numTiles * 6);

  // Get old mvMat
  tw.mat4.copy(tw.mvMat, tw.tmpMat);

  // keep textures disabled by default
  tw.sTexCoords(0);
}

tw.vertexShader = [
  "#ifdef GL_FRAGMENT_PRECISION_HIGH",
  "precision highp float;",
  "#else",
  "precision mediump float;",
  "#endif",
  "attribute vec2 aPosition;",
  "attribute vec4 aVertexColor;",
  "attribute vec2 aTexCoord;",
  "uniform mat4 uPMatrix;",
  "uniform mat4 uMVMatrix;",
  "uniform bool uTexCoord;",
  "uniform bool uVertexColor;",
  "varying vec2 vTexCoord;",
  "varying vec4 vColor;",
  "void main(void)",
  "{",
  "  gl_Position = uPMatrix * uMVMatrix * vec4(aPosition.x, aPosition.y, 0.0, 1.0);",
  "  if (uTexCoord)",
  "    vTexCoord = aTexCoord;",
  "  if (uVertexColor)",
  "    vColor = aVertexColor;",
  "}"
].join("\n");

tw.fragmentShader = [
  "#ifdef GL_FRAGMENT_PRECISION_HIGH",
  "precision highp float;",
  "#else",
  "precision mediump float;",
  "#endif",
  "varying vec2 vTexCoord;",
  "varying vec4 vColor;",
  "uniform sampler2D uSampler;",
  "uniform vec4 uColorMask;",
  "uniform bool uTexCoord;",
  "uniform bool uVertexColor;",
  "void main(void)",
  "{",
  "  gl_FragColor = uColorMask;",
  "  if (uTexCoord)",
  "    gl_FragColor *= texture2D(uSampler, vec2(vTexCoord.s, vTexCoord.t));",
  "  if (uVertexColor)",
  "    gl_FragColor *= vColor;",
  "}"
].join("\n");

