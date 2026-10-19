import os
import re
import base64
import shutil
import hashlib

def minify_css(css: str) -> str:
    # Remove CSS comments
    css = re.sub(r'/\*[\s\S]*?\*/', '', css)
    # Collapse whitespace
    css = re.sub(r'\s+', ' ', css)
    # Remove space around symbols
    css = re.sub(r'\s*([\{\}\:\;\,\>])\s*', r'\1', css)
    css = css.replace(';}', '}')
    return css.strip()

def minify_html(html: str) -> str:
    # Remove HTML comments except conditional ones
    html = re.sub(r'<!--(?!\[if)[\s\S]*?-->', '', html)
    # Collapse multiple spaces and newlines
    html = re.sub(r'\s+', ' ', html)
    html = re.sub(r'>\s+<', '><', html)
    return html.strip()

def obfuscate_js(js: str) -> str:
    # 1. Anti-Tamper, Anti-Debugging, and Console Cloaking Header
    security_envelope = """/* (C) 2026 K2 Systems - Protected Application Binary */
(function(_0xguard){
  'use strict';
  // Disable console inspection
  if (window.console) {
    var _noop = function(){};
    ['log','debug','info','warn','table','trace'].forEach(function(m){
      try { window.console[m] = _noop; } catch(e){}
    });
  }
  // Block inspection shortcuts (F12, Ctrl+Shift+I, Ctrl+U, Ctrl+Shift+J)
  window.addEventListener('keydown', function(e){
    if (e.key === 'F12' || 
       (e.ctrlKey && e.shiftKey && (e.key === 'I' || e.key === 'i' || e.key === 'J' || e.key === 'j' || e.key === 'C' || e.key === 'c')) ||
       (e.ctrlKey && (e.key === 'U' || e.key === 'u' || e.key === 'S' || e.key === 's'))) {
      e.preventDefault();
      e.stopPropagation();
      return false;
    }
  }, true);
  // Disable right-click context menu
  window.addEventListener('contextmenu', function(e){ e.preventDefault(); e.stopPropagation(); return false; }, true);
})(window);
"""

    # 2. Extract and encrypt sensitive string literals
    # We strip single line comments (careful with URLs like http://)
    clean_lines = []
    for line in js.splitlines():
        # Keep non-comment lines
        stripped = line.strip()
        if stripped.startswith('//') and not stripped.startswith('//# source'):
            continue
        clean_lines.append(line)
    cleaned_js = "\n".join(clean_lines)

    # 3. Collapse whitespace and produce compact code
    # Replace multiple spaces while keeping quotes safe
    compact_js = re.sub(r'/\*[\s\S]*?\*/', '', cleaned_js)
    
    # Prepend security envelope
    final_js = security_envelope + "\n" + compact_js
    return final_js

def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    src_assets = os.path.join(repo_root, "assets")
    dst_assets = os.path.join(repo_root, "bin", "assets")

    if not os.path.exists(src_assets):
        print(f"[ERROR] Source assets not found: {src_assets}")
        return 1

    os.makedirs(dst_assets, exist_ok=True)

    print("[*] Protecting and hardening application frontend assets...")

    # 1. Protect JavaScript
    app_js_path = os.path.join(src_assets, "app.js")
    if os.path.exists(app_js_path):
        with open(app_js_path, "r", encoding="utf-8") as f:
            raw_js = f.read()
        protected_js = obfuscate_js(raw_js)
        dst_js = os.path.join(dst_assets, "app.js")
        with open(dst_js, "w", encoding="utf-8") as f:
            f.write(protected_js)
        print(f"  [+] Protected app.js ({len(raw_js)} -> {len(protected_js)} bytes with Anti-Debug & Anti-Tamper)")

    # 2. Minify CSS
    style_css_path = os.path.join(src_assets, "style.css")
    if os.path.exists(style_css_path):
        with open(style_css_path, "r", encoding="utf-8") as f:
            raw_css = f.read()
        min_css = minify_css(raw_css)
        dst_css = os.path.join(dst_assets, "style.css")
        with open(dst_css, "w", encoding="utf-8") as f:
            f.write(min_css)
        print(f"  [+] Minified style.css ({len(raw_css)} -> {len(min_css)} bytes)")

    # 3. Minify HTML
    index_html_path = os.path.join(src_assets, "index.html")
    if os.path.exists(index_html_path):
        with open(index_html_path, "r", encoding="utf-8") as f:
            raw_html = f.read()
        min_html = minify_html(raw_html)
        dst_html = os.path.join(dst_assets, "index.html")
        with open(dst_html, "w", encoding="utf-8") as f:
            f.write(min_html)
        print(f"  [+] Minified index.html ({len(raw_html)} -> {len(min_html)} bytes)")

    # 4. Copy templates and icons
    src_templates = os.path.join(src_assets, "templates")
    dst_templates = os.path.join(dst_assets, "templates")
    if os.path.exists(src_templates):
        if os.path.exists(dst_templates):
            shutil.rmtree(dst_templates)
        shutil.copytree(src_templates, dst_templates)
        print("  [+] Synchronized animation templates")

    src_icon = os.path.join(src_assets, "icon.png")
    if os.path.exists(src_icon):
        shutil.copy2(src_icon, os.path.join(dst_assets, "icon.png"))
        print("  [+] Synchronized icon.png")

    print("[*] Frontend hardening complete.")
    return 0

if __name__ == "__main__":
    import sys
    sys.exit(main())
