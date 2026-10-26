#ifndef PORTAL_HTML_H
#define PORTAL_HTML_H

#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

// =============================================================================
// GENERATOR HALAMAN UTAMA CAPTIVE PORTAL SETUP
// =============================================================================
inline String generatePortalRootHtml(const String &savedSsid) {
  int n = WiFi.scanNetworks();
  String wifiListHtml = "";

  if (n <= 0) {
    wifiListHtml = "<div class='wifi-item'><span style='color:#94a3b8;'>Tidak ada Wi-Fi terdeteksi. Silakan ketik nama Wi-Fi manual di bawah.</span></div>";
  } else {
    for (int i = 0; i < n; ++i) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      String security = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "🔓 Terbuka" : "🔒 WPA2";
      
      String signalColor = "#22c55e"; // Hijau
      String signalText = "Kuat";
      if (rssi < -75) { signalColor = "#ef4444"; signalText = "Lemah"; }
      else if (rssi < -60) { signalColor = "#eab308"; signalText = "Sedang"; }

      wifiListHtml += "<div class='wifi-item' onclick='selectWifi(\"" + ssid + "\")'>";
      wifiListHtml += "  <div class='wifi-info'>";
      wifiListHtml += "    <div class='wifi-name'>" + ssid + "</div>";
      wifiListHtml += "    <div class='wifi-meta'>" + security + " &bull; <span style='color:" + signalColor + "; font-weight:600;'>" + signalText + " (" + String(rssi) + " dBm)</span></div>";
      wifiListHtml += "  </div>";
      wifiListHtml += "  <button type='button' class='btn-select'>PILIH</button>";
      wifiListHtml += "</div>";
    }
  }

  String currentSsidDisplay = (savedSsid == "") ? "Belum Terhubung" : savedSsid;

  String html = "<!DOCTYPE html><html lang='id'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP32 Edge AI - Pengaturan Wi-Fi</title>";
  html += "<style>";
  html += "* { box-sizing: border-box; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }";
  html += "body { background: #0b1120; color: #f8fafc; margin: 0; padding: 16px; display: flex; justify-content: center; }";
  html += ".container { max-width: 440px; width: 100%; background: #1e293b; border: 1px solid #334155; border-radius: 20px; padding: 22px; box-shadow: 0 20px 25px -5px rgba(0,0,0,0.5); }";
  html += ".header { text-align: center; margin-bottom: 20px; }";
  html += ".header h2 { margin: 6px 0; color: #38bdf8; font-size: 22px; }";
  html += ".header p { margin: 0; color: #94a3b8; font-size: 13px; }";
  html += ".info-box { background: #0f172a; border: 1px solid #334155; border-radius: 12px; padding: 14px; margin-bottom: 18px; font-size: 12px; }";
  html += ".info-row { display: flex; justify-content: space-between; margin-bottom: 6px; }";
  html += ".info-row:last-child { margin-bottom: 0; }";
  html += ".info-label { color: #94a3b8; }";
  html += ".info-value { color: #38bdf8; font-weight: 600; }";
  html += ".section-title { font-size: 13px; font-weight: 700; color: #cbd5e1; text-transform: uppercase; margin: 16px 0 8px; letter-spacing: 0.5px; }";
  html += ".wifi-list { max-height: 190px; overflow-y: auto; background: #0f172a; border: 1px solid #334155; border-radius: 12px; padding: 6px; margin-bottom: 16px; }";
  html += ".wifi-item { display: flex; justify-content: space-between; align-items: center; padding: 10px; border-radius: 8px; cursor: pointer; transition: 0.2s; border-bottom: 1px solid #1e293b; }";
  html += ".wifi-item:hover { background: #1e293b; }";
  html += ".wifi-name { font-size: 14px; font-weight: 600; color: #f1f5f9; }";
  html += ".wifi-meta { font-size: 11px; color: #64748b; margin-top: 2px; }";
  html += ".btn-select { background: #0284c7; color: white; border: none; padding: 6px 12px; border-radius: 6px; font-size: 11px; font-weight: 700; cursor: pointer; }";
  html += "label { font-size: 12px; font-weight: 600; color: #94a3b8; display: block; margin-top: 12px; }";
  html += "input[type='text'], input[type='password'] { width: 100%; padding: 12px; margin-top: 5px; border: 1px solid #475569; border-radius: 8px; background: #0f172a; color: #fff; font-size: 14px; }";
  html += "input[type='submit'] { width: 100%; background: linear-gradient(135deg, #0284c7, #2563eb); color: white; border: none; padding: 14px; border-radius: 10px; font-weight: 700; font-size: 15px; margin-top: 20px; cursor: pointer; }";
  html += ".tips { margin-top: 16px; padding: 12px; background: rgba(56, 189, 248, 0.08); border-left: 3px solid #38bdf8; border-radius: 6px; font-size: 12px; color: #94a3b8; line-height: 1.5; }";
  html += "</style>";
  html += "<script>";
  html += "function selectWifi(name) {";
  html += "  document.getElementById('ssid_input').value = name;";
  html += "  document.getElementById('password_input').focus();";
  html += "}";
  html += "</script></head><body>";
  
  html += "<div class='container'>";
  html += "  <div class='header'>";
  html += "    <h2>💧 ESP32 Water AI Setup</h2>";
  html += "    <p>Pengaturan Jaringan & Telemetri Kualitas Air</p>";
  html += "  </div>";

  html += "  <div class='info-box'>";
  html += "    <div class='info-row'><span class='info-label'>Status Wi-Fi Saat Ini</span><span class='info-value'>" + currentSsidDisplay + "</span></div>";
  html += "    <div class='info-row'><span class='info-label'>IP Gateway Hotspot</span><span class='info-value'>192.168.4.1</span></div>";
  html += "    <div class='info-row'><span class='info-label'>MAC Address ESP32</span><span class='info-value'>" + WiFi.macAddress() + "</span></div>";
  html += "    <div class='info-row'><span class='info-label'>Edge AI Classifier</span><span class='info-value' style='color:#22c55e;'>100% Offline Ready</span></div>";
  html += "  </div>";

  html += "  <div class='section-title'>PILIH WI-FI 2.4 GHz TERSEDIA:</div>";
  html += "  <div class='wifi-list'>" + wifiListHtml + "</div>";

  html += "  <form method='POST' action='/save'>";
  html += "    <label>NAMA WI-FI (SSID):</label>";
  html += "    <input type='text' id='ssid_input' name='ssid' value='" + savedSsid + "' placeholder='Pilih dari daftar atau ketik manual...' required>";

  html += "    <label>PASSWORD WI-FI:</label>";
  html += "    <div style='position:relative; display:flex; align-items:center;'>";
  html += "      <input type='password' id='password_input' name='password' placeholder='Masukkan password Wi-Fi...' style='padding-right:45px;'>";
  html += "      <button type='button' id='eye_btn' onclick='togglePassword()' style='position:absolute; right:10px; background:none; border:none; color:#94a3b8; cursor:pointer; font-size:18px; padding:4px; outline:none;'>👁️</button>";
  html += "    </div>";

  html += "    <input type='submit' value='⚡ HUBUNGKAN ESP32 KE WI-FI'>";
  html += "  </form>";

  html += "<script>";
  html += "function togglePassword() {";
  html += "  var pw = document.getElementById('password_input');";
  html += "  var btn = document.getElementById('eye_btn');";
  html += "  if (pw.type === 'password') {";
  html += "    pw.type = 'text';";
  html += "    btn.innerText = '🙈';";
  html += "  } else {";
  html += "    pw.type = 'password';";
  html += "    btn.innerText = '👁️';";
  html += "  }";
  html += "}";
  html += "</script>";

  html += "  <div class='tips'>";
  html += "    💡 <b>Catatan Penting:</b><br>";
  html += "    &bull; ESP32 hanya mendukung sinyal <b>Wi-Fi 2.4 GHz</b>.<br>";
  html += "    &bull; Wi-Fi bersifat opsional! Uji klasifikasi kualitas air fisik tetap bisa berjalan mandiri via Push Button (D32).";
  html += "  </div>";
  html += "</div></body></html>";

  return html;
}

// =============================================================================
// GENERATOR HALAMAN SUKSES KONEKSI WI-FI
// =============================================================================
inline String generateSuccessHtml(const String &ssid, const String &ip) {
  String html = "<!DOCTYPE html><html lang='id'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>body{background:#0b1120;color:#fff;font-family:sans-serif;padding:24px;text-align:center;}";
  html += ".card{background:#1e293b;border-radius:18px;padding:28px;max-width:400px;margin:auto;border:1px solid #22c55e;box-shadow:0 10px 25px rgba(0,0,0,0.5);}";
  html += "h2{color:#22c55e;margin-top:0;} .ip-box{background:#0f172a;padding:14px;border-radius:10px;font-size:18px;font-weight:bold;color:#38bdf8;margin:16px 0;border:1px solid #334155;}</style></head><body>";
  html += "<div class='card'>";
  html += "  <h2>✅ BERHASIL TERHUBUNG!</h2>";
  html += "  <p>ESP32 kini sudah resmi terhubung ke Wi-Fi:</p>";
  html += "  <h3 style='color:#f8fafc;'>" + ssid + "</h3>";
  html += "  <div class='ip-box'>IP ESP32: " + ip + "</div>";
  html += "  <p style='color:#94a3b8;font-size:13px;'>Port TCP Telemetri: <b>8080</b></p>";
  html += "  <p style='color:#cbd5e1;font-size:13px;line-height:1.5;'>Hotspot setup akan dinonaktifkan. Silakan hubungkan kembali HP/Laptop Anda ke Wi-Fi <b>" + ssid + "</b> dan jalankan <code>live_viewer.py</code> di laptop!</p>";
  html += "</div></body></html>";
  return html;
}

// =============================================================================
// GENERATOR HALAMAN GAGAL KONEKSI WI-FI
// =============================================================================
inline String generateFailHtml(const String &ssid) {
  String html = "<!DOCTYPE html><html lang='id'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>body{background:#0b1120;color:#fff;font-family:sans-serif;padding:24px;text-align:center;}";
  html += ".card{background:#1e293b;border-radius:18px;padding:28px;max-width:400px;margin:auto;border:1px solid #ef4444;}";
  html += "h2{color:#ef4444;margin-top:0;} a{display:inline-block;background:#0284c7;color:#fff;padding:12px 24px;border-radius:8px;text-decoration:none;font-weight:bold;margin-top:16px;}</style></head><body>";
  html += "<div class='card'>";
  html += "  <h2>❌ GAGAL TERHUBUNG</h2>";
  html += "  <p>ESP32 tidak dapat terhubung ke <b>" + ssid + "</b>.</p>";
  html += "  <p style='color:#94a3b8;font-size:13px;text-align:left;'><b>Kemungkinan penyebab:</b><br>1. Password Wi-Fi salah.<br>2. Sinyal Wi-Fi router terlalu jauh/lemah.<br>3. Jaringan adalah 5 GHz murni (ESP32 butuh 2.4 GHz).</p>";
  html += "  <a href='/'>⬅️ KEMBALI & COBA LAGI</a>";
  html += "</div></body></html>";
  return html;
}

#endif // PORTAL_HTML_H
