----------------------------------------------------------------------
-- gradient.lua: creating and editing color gradients
----------------------------------------------------------------------
--[[

    This file is part of the extensible drawing editor Ipe.

    Ipe is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Gradients in Ipe live in a style sheet and are interpreted in the
    coordinate system of the object that uses them.  The gradient editor
    describes a gradient relative to the object's bounding box (angle,
    center, size in percent), and this file converts that description
    into a gradient definition fitted to each selected object.  The
    definitions are stored in a document style sheet named "gradients".

--]]

local GRADIENT_SHEET = "gradients"
local GRADIENT_PREFIX = "grad"

-- remembered for "Apply last gradient"
local last_spec = nil

----------------------------------------------------------------------

local function fmt(x)
  local s = string.format("%.4f", x)
  s = s:gsub("0+$", ""):gsub("%.$", "")
  if s == "-0" then s = "0" end
  return s
end

-- bounding box of the object in its own coordinate system
local function local_bbox(obj)
  local r = ipe.Rect()
  obj:addToBBox(r, obj:matrix():inverse(), false)
  return r
end

-- geometry of the gradient for a given box (must match ipeui_gradient_qt.cpp)
local function gradient_coords(spec, box)
  local x0, y0 = box:left(), box:bottom()
  local w, h = box:width(), box:height()
  if w < 1e-6 then w = 1e-6 end
  if h < 1e-6 then h = 1e-6 end
  if spec.type == "radial" then
    local R = (spec.radius or 1) * 0.5 * math.max(w, h)
    local px = x0 + (spec.cx or 0.5) * w
    local py = y0 + (spec.cy or 0.5) * h
    local fx = px + (spec.fx or 0) * R
    local fy = py + (spec.fy or 0) * R
    return string.format("%s %s 0 %s %s %s", fmt(fx), fmt(fy),
			 fmt(px), fmt(py), fmt(R))
  else
    local a = math.rad(spec.angle or 0)
    local dx, dy = math.cos(a), math.sin(a)
    local L = 0.5 * (math.abs(w * dx) + math.abs(h * dy))
    local cx, cy = x0 + 0.5 * w, y0 + 0.5 * h
    return string.format("%s %s %s %s", fmt(cx - L * dx), fmt(cy - L * dy),
			 fmt(cx + L * dx), fmt(cy + L * dy))
  end
end

local function gradient_xml(name, spec, box)
  local t = { }
  t[#t+1] = string.format('<gradient name="%s" type="%s" extend="%s" coords="%s">',
			  name, spec.type == "radial" and "radial" or "axial",
			  spec.extend and "yes" or "no", gradient_coords(spec, box))
  for _, s in ipairs(spec.stops) do
    local c = s.color
    local op = ""
    if s.opacity and s.opacity < 0.9995 then op = ' opacity="' .. fmt(s.opacity) .. '"' end
    t[#t+1] = string.format('<stop offset="%s" color="%s %s %s"%s/>', fmt(s.offset),
			    fmt(c.r), fmt(c.g), fmt(c.b), op)
  end
  t[#t+1] = "</gradient>"
  return table.concat(t, "\n")
end

-- reconstruct an editor description from an existing gradient definition
local function spec_from_gradient(g, box)
  local w, h = math.max(box:width(), 1e-6), math.max(box:height(), 1e-6)
  local spec = { extend = g.extend, stops = { } }
  for _, s in ipairs(g.stops) do
    spec.stops[#spec.stops + 1] = { offset = s.offset, color = s.color,
				    opacity = s.opacity or 1 }
  end
  local v1, v2 = g.v[1], g.v[2]
  if g.type == "radial" then
    spec.type = "radial"
    local R = math.max(g.radius[2], 1e-6)
    spec.cx = (v2.x - box:left()) / w
    spec.cy = (v2.y - box:bottom()) / h
    spec.radius = R / (0.5 * math.max(w, h))
    spec.fx = (v1.x - v2.x) / R
    spec.fy = (v1.y - v2.y) / R
  else
    spec.type = "linear"
    local d = v2 - v1
    spec.angle = math.floor(math.deg(math.atan(d.y, d.x)) + 0.5)
  end
  return spec
end

local function find_gradient_sheet(sheets)
  for i = 1, sheets:count() do
    if sheets:sheet(i):name() == GRADIENT_SHEET then return sheets:sheet(i), i end
  end
  return nil
end

-- names of all gradients used by any object in the document
local function used_gradients(doc, skip_pno)
  local used = { }
  local function visit(obj)
    local ty = obj:type()
    if ty == "path" then
      local g = obj:get("gradient")
      if g and g ~= "normal" then used[g] = true end
      local sg = obj:get("strokegradient")
      if sg and sg ~= "normal" then used[sg] = true end
    elseif ty == "group" then
      for _, e in ipairs(obj:elements()) do visit(e) end
    end
  end
  for pno, p in doc:pages() do
    if pno ~= skip_pno then
      for _, obj in p:objects() do visit(obj) end
    end
  end
  return used
end

local function unique_name(sheets, taken)
  local existing = { }
  for _, n in ipairs(sheets:allNames("gradient")) do existing[n] = true end
  local i = 1
  while true do
    local name = GRADIENT_PREFIX .. i
    if not existing[name] and not taken[name] then return name end
    i = i + 1
  end
end

local function absolute_color(sheets, c)
  if type(c) == "table" then return c end
  if type(c) == "string" then
    local abs = sheets:find("color", c)
    if type(abs) == "table" then return abs end
  end
  return { r = 0, g = 0, b = 0 }
end

----------------------------------------------------------------------

-- Is the path a closed shape (so that filling it makes sense)?
local function is_closed(obj)
  for _, sp in ipairs(obj:shape()) do
    if sp.type == "curve" and not sp.closed then return false end
  end
  return true
end

-- absolute value of an opacity attribute (symbolic name or "opaque")
local function absolute_opacity(sheets, name)
  if type(name) == "number" then return name end
  if not name or name == "opaque" or name == "normal" then return 1 end
  local v = sheets:find("opacity", name)
  if type(v) == "number" then return v end
  return 1
end

-- symbolic opacity with the given value: reuse one from the style sheets,
-- or define a new one in the gradient sheet
local function opacity_name(sheets, gsheet, value)
  if value >= 0.9995 then return "opaque" end
  for _, n in ipairs(sheets:allNames("opacity")) do
    local v = sheets:find("opacity", n)
    if type(v) == "number" and math.abs(v - value) < 0.0005 then return n end
  end
  local name = fmt(value * 100) .. "%"
  gsheet:add("opacity", name, value)
  return name
end

-- Selected path objects on the current page (indices).
-- Open polylines are only used if they are already filled, or for outlines.
function MODEL:gradient_targets(for_stroke)
  local p = self:page()
  local list = { }
  for i, obj, sel, layer in p:objects() do
    if sel and obj:type() == "path" then
      if for_stroke or obj:get("pathmode") ~= "stroked" or is_closed(obj) then
	list[#list + 1] = i
      end
    end
  end
  return list
end

-- Apply 'spec' to the selected paths, as one undoable step
function MODEL:apply_gradient_spec(spec, label)
  local targets = self:gradient_targets(spec.target == "stroke")
  if #targets == 0 then
    self:warning("Select one or more path objects (rectangles, circles, polygons...) first.")
    return
  end
  local p = self:page()
  local sheets = self.doc:sheets():clone()
  local gsheet = find_gradient_sheet(sheets)
  if not gsheet then
    local s = ipe.Sheet(nil, '<ipestyle name="' .. GRADIENT_SHEET .. '"></ipestyle>')
    sheets:insert(1, s)
    gsheet = find_gradient_sheet(sheets)
  end

  local final = p:clone()
  local taken = { }
  local xml = { '<ipestyle name="tmp">' }
  local names = { }
  for _, i in ipairs(targets) do
    local obj = final[i]
    local name = unique_name(sheets, taken)
    taken[name] = true
    names[i] = name
    xml[#xml + 1] = gradient_xml(name, spec, local_bbox(obj))
  end
  xml[#xml + 1] = "</ipestyle>"
  local tmp, msg = ipe.Sheet(nil, table.concat(xml, "\n"))
  if not tmp then
    self:warning("Could not create the gradient", msg)
    return
  end
  local opname = nil
  if spec.opacity then opname = opacity_name(sheets, gsheet, spec.opacity) end
  local opprop = (spec.target == "stroke") and "strokeopacity" or "opacity"
  for _, i in ipairs(targets) do
    local obj = final[i]
    gsheet:addFrom(tmp, "gradient", names[i])
    if opname then final:setAttribute(i, opprop, opname) end
    local mode = obj:get("pathmode")
    if spec.target == "stroke" then
      final:setAttribute(i, "strokegradient", names[i])
      if mode == "filled" then final:setAttribute(i, "pathmode", "strokedfilled") end
    else
      final:setAttribute(i, "gradient", names[i])
      if mode == "stroked" then final:setAttribute(i, "pathmode", "strokedfilled") end
    end
  end

  -- drop gradients of this sheet that nothing uses any more
  local used = used_gradients(self.doc, self.pno)
  for _, obj in final:objects() do
    if obj:type() == "path" then
      used[obj:get("gradient")] = true
      used[obj:get("strokegradient")] = true
    elseif obj:type() == "group" then
      for _, e in ipairs(obj:elements()) do
	if e:type() == "path" then
	  used[e:get("gradient")] = true
	  used[e:get("strokegradient")] = true
	end
      end
    end
  end
  for _, n in ipairs(gsheet:allNames("gradient")) do
    if not used[n] then gsheet:remove("gradient", n) end
  end

  local t = { label = label or "set gradient",
	      pno = self.pno,
	      vno = self.vno,
	      style_sheets_changed = true,
	      final_sheets = sheets,
	      original_page = p:clone(),
	      final_page = final,
	    }
  t.redo = function (t, doc)
	     t.original_sheets = doc:replaceSheets(t.final_sheets)
	     doc:set(t.pno, t.final_page)
	   end
  t.undo = function (t, doc)
	     t.final_sheets = doc:replaceSheets(t.original_sheets)
	     doc:set(t.pno, t.original_page)
	   end
  self:register(t)
  last_spec = spec
end

----------------------------------------------------------------------
-- Actions (menu "Gradient")
----------------------------------------------------------------------

function MODEL:action_gradient_edit()
  if not ipeui.gradientDialog then
    self:warning("The gradient editor is only available in the Qt version of Ipe.")
    return
  end
  local targets = self:gradient_targets(true)
  if #targets == 0 then
    self:warning("Select one or more path objects (rectangles, circles, polygons...) first.")
    return
  end
  local p = self:page()
  local prim = p:primarySelection()
  if not prim or p[prim]:type() ~= "path" then prim = targets[1] end
  local obj = p[prim]
  local sheets = self.doc:sheets()

  -- start from the object's current gradient, if it has one
  local spec = nil
  local current = obj:get("gradient")
  local target = "fill"
  if (not current or current == "normal") and obj:get("strokegradient") ~= "normal" then
    current = obj:get("strokegradient")
    target = "stroke"
  end
  if current and current ~= "normal" then
    local g = sheets:find("gradient", current)
    if g then spec = spec_from_gradient(g, local_bbox(obj)) end
  end
  if not spec then
    if last_spec then
      spec = { }
      for k, v in pairs(last_spec) do spec[k] = v end
    else
      local base = absolute_color(sheets, obj:get("fill"))
      if obj:get("pathmode") == "stroked" then
	base = absolute_color(sheets, obj:get("stroke"))
      end
      if base.r > 0.95 and base.g > 0.95 and base.b > 0.95 then
	base = { r = 0.12, g = 0.53, b = 0.9 }
      end
      spec = { type = "linear", angle = 90, extend = true,
	       stops = { { offset = 0, color = base, opacity = 1 },
			 { offset = 1, color = { r = 1, g = 1, b = 1 }, opacity = 1 } } }
    end
  end
  spec.target = target
  spec.allow_stroke = true
  -- keep the object's current transparency
  spec.opacity = absolute_opacity(sheets,
				  obj:get(target == "stroke" and "strokeopacity" or "opacity"))

  local initial_opacity = spec.opacity
  local result = ipeui.gradientDialog(self.ui:win(), spec)
  if result then
    -- only touch the opacity if it was changed in the dialog, so that
    -- every selected object keeps its own transparency
    if math.abs((result.opacity or 1) - initial_opacity) < 0.005 then
      result.opacity = nil
    end
    self:apply_gradient_spec(result, "set gradient")
  end
end

function MODEL:action_gradient_apply_last()
  if not last_spec then
    self:action_gradient_edit()
    return
  end
  self:apply_gradient_spec(last_spec, "apply gradient")
end

-- Re-fit the existing gradients of the selection to the current shape
-- (useful after editing the vertices of a path)
function MODEL:action_gradient_refit()
  local targets = self:gradient_targets(true)
  local p = self:page()
  local sheets = self.doc:sheets()
  local count = 0
  for _, i in ipairs(targets) do
    local g = p[i]:get("gradient")
    if g and g ~= "normal" then count = count + 1 end
  end
  if count == 0 then
    self:warning("None of the selected paths has a gradient fill.")
    return
  end
  -- refit each object separately, with its own description
  local prim = targets[1]
  local g = sheets:find("gradient", p[prim]:get("gradient"))
  if not g then return end
  -- all selected objects get the primary object's gradient, fitted to their own shape
  local spec = spec_from_gradient(g, local_bbox(p[prim]))
  spec.target = "fill"
  -- spec.opacity stays nil: each object keeps its own opacity
  self:apply_gradient_spec(spec, "fit gradient to shape")
end

function MODEL:action_gradient_remove()
  local targets = self:gradient_targets(true)
  if #targets == 0 then
    self:warning("Select the objects whose gradient should be removed.")
    return
  end
  local p = self:page()
  local t = { label = "remove gradient",
	      pno = self.pno,
	      vno = self.vno,
	      selection = targets,
	      original = p:clone(),
	      undo = revertOriginal,
	    }
  t.redo = function (t, doc)
	     local p = doc[t.pno]
	     for _, i in ipairs(t.selection) do
	       p:setAttribute(i, "gradient", "normal")
	       p:setAttribute(i, "strokegradient", "normal")
	     end
	   end
  self:register(t)
end

----------------------------------------------------------------------
