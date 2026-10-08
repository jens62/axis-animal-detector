"use strict";

// The camera's web server runs this page and param.cgi on the same origin, so the
// browser's existing camera login is used. Everything is written with textContent
// or .value, never innerHTML.
const GROUP = "root.animal_detector";
const PARAM_CGI = "/axis-cgi/param.cgi";

const form = document.getElementById("form");
const status = document.getElementById("status");
const fields = Array.from(document.querySelectorAll("[data-param]"));
const simButtons = document.getElementById("sim-buttons");

let loaded = {};  // values as read from the camera, to send only what changed

function setStatus(text, kind) {
  status.textContent = text;
  status.className = kind || "";
}

// The camera capitalizes the group name, so match case-insensitively.
function parseParams(text, group) {
  const values = {};
  const prefix = (group + ".").toLowerCase();
  for (const line of text.split(/\r?\n/)) {
    const eq = line.indexOf("=");
    if (eq > 0 && line.slice(0, prefix.length).toLowerCase() === prefix)
      values[line.slice(prefix.length, eq)] = line.slice(eq + 1);
  }
  return values;
}

async function listGroup(group) {
  const res = await fetch(PARAM_CGI + "?action=list&group=" + encodeURIComponent(group),
                          { credentials: "same-origin" });
  if (!res.ok) throw new Error("HTTP " + res.status);
  return parseParams(await res.text(), group);
}

async function updateParams(changes) {
  const body = new URLSearchParams({ action: "update" });
  for (const [key, value] of Object.entries(changes)) body.set(GROUP + "." + key, value);
  const res = await fetch(PARAM_CGI, {
    method: "POST",
    credentials: "same-origin",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body,
  });
  const text = (await res.text()).trim();
  if (!res.ok || !text.startsWith("OK")) throw new Error(text || "HTTP " + res.status);
}

function readField(el) {
  return el.type === "checkbox" ? (el.checked ? "yes" : "no") : el.value.trim();
}

function writeField(el, value) {
  if (el.type === "checkbox") el.checked = value === "yes";
  else el.value = value;
}

// One button per configured (saved) animal class.
async function simulate(name) {
  setStatus("Simulating " + name + "…");
  try {
    // The nonce makes the value change every time, so the app is notified for repeated clicks.
    await updateParams({ Simulate: name + " " + Date.now() });
    setStatus("Simulated " + name + ".", "ok");
  } catch (err) {
    setStatus("Could not simulate: " + err.message, "error");
  }
}

function buildSimButtons(classes) {
  simButtons.replaceChildren();
  for (const name of classes.split(",").map((s) => s.trim()).filter((s) => s !== "")) {
    const button = document.createElement("button");
    button.type = "button";
    button.textContent = name.charAt(0).toUpperCase() + name.slice(1);
    button.addEventListener("click", () => simulate(name));
    simButtons.appendChild(button);
  }
}

async function load() {
  setStatus("Loading…");
  try {
    loaded = await listGroup(GROUP);
    if (!("Threshold" in loaded)) throw new Error("parameters not found, is the app installed?");
    for (const el of fields) writeField(el, loaded[el.dataset.param] ?? "");
    buildSimButtons(loaded.AnimalClasses ?? "");
    setStatus("");
  } catch (err) {
    setStatus("Could not load settings: " + err.message, "error");
  }
}

function validate(changes) {
  const num = (key, min, max) => {
    if (!(key in changes)) return null;
    const n = Number(changes[key]);
    return Number.isFinite(n) && n >= min && n <= max ? null : key + " must be a number from " + min + " to " + max;
  };
  return num("Threshold", 1, 100) || num("StartFrames", 1, 1000) || num("HoldSec", 0, 3600) || num("DebugThreshold", 0, 100) ||
         num("MinBoxPct", 1, 50) || num("MinAnimalPct", 0, 50) || num("RegionHoldSec", 0, 60) || num("MaxRegions", 1, 8);
}

async function save(event) {
  event.preventDefault();

  const changes = {};
  for (const el of fields) {
    const key = el.dataset.param;
    const value = readField(el);
    if (value !== (loaded[key] ?? "")) changes[key] = value;
  }
  if (Object.keys(changes).length === 0) {
    setStatus("Nothing changed.");
    return;
  }
  const problem = validate(changes);
  if (problem) {
    setStatus(problem, "error");
    return;
  }

  setStatus("Saving…");
  try {
    await updateParams(changes);
    setStatus("Saved. The application restarts to apply the settings.", "ok");
    setTimeout(load, 2000);
  } catch (err) {
    setStatus("Could not save: " + err.message, "error");
  }
}

// defaults.json is generated from manifest.json when the package is built.
async function resetToDefaults() {
  if (!confirm("Reset all settings to the defaults?")) return;
  try {
    const res = await fetch("defaults.json", { credentials: "same-origin" });
    if (!res.ok) throw new Error("HTTP " + res.status);
    const defaults = await res.json();
    for (const el of fields) writeField(el, defaults[el.dataset.param] ?? "");
    setStatus("Defaults loaded. Press Save to apply them.");
  } catch (err) {
    setStatus("Could not load the defaults: " + err.message, "error");
  }
}

function exportSettings() {
  const settings = {};
  for (const el of fields) settings[el.dataset.param] = readField(el);
  const blob = new Blob([JSON.stringify(settings, null, 2) + "\n"], { type: "application/json" });
  const link = document.createElement("a");
  link.href = URL.createObjectURL(blob);
  link.download = "animal-detector-settings.json";
  link.click();
  URL.revokeObjectURL(link.href);
  setStatus("Settings exported.");
}

async function importSettings(file) {
  try {
    const settings = JSON.parse(await file.text());
    let count = 0;
    for (const el of fields) {
      const key = el.dataset.param;
      if (typeof settings[key] === "string") {
        writeField(el, settings[key]);
        count++;
      }
    }
    if (count === 0) throw new Error("no known settings in this file");
    setStatus("Imported " + count + " settings. Press Save to apply them.");
  } catch (err) {
    setStatus("Could not import: " + err.message, "error");
  }
}

document.getElementById("reset").addEventListener("click", resetToDefaults);
document.getElementById("export").addEventListener("click", exportSettings);
const importFile = document.getElementById("import-file");
document.getElementById("import").addEventListener("click", () => importFile.click());
importFile.addEventListener("change", () => {
  if (importFile.files.length > 0) importSettings(importFile.files[0]);
  importFile.value = "";
});

form.addEventListener("submit", save);
document.getElementById("reload").addEventListener("click", load);
document.addEventListener("DOMContentLoaded", load);
