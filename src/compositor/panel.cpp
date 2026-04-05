#define GLEW_STATIC
#include "compositor.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <Imlib2.h>

void Compositor::init_panel() {
    _pan_font_title  = XftFontOpenName(_dpy, _screen, "sans:bold:size=14");
    _pan_font_artist = XftFontOpenName(_dpy, _screen, "sans:size=12");
    _pan_font_time   = XftFontOpenName(_dpy, _screen, "monospace:size=12");
    if (!_pan_font_title) _pan_font_title = XftFontOpenName(_dpy, _screen, "fixed:size=14");

    glGenTextures(1, &_pan_bg_tex);
    glBindTexture(GL_TEXTURE_2D, _pan_bg_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, PAN_W, PAN_H,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    make_fbo(PAN_W, PAN_H, _pan_blur_fbo, _pan_blur_tex);
}

void Compositor::load_art(const std::string& url) {
    if (url == _art_last_url) return;
    _art_last_url = url;
    if (_art_tex) { glDeleteTextures(1, &_art_tex); _art_tex = 0; }
    if (url.empty()) return;
    std::string path = url;
    if (path.rfind("file://", 0) == 0) path = path.substr(7);
    Imlib_Image img = imlib_load_image(path.c_str());
    if (!img) return;
    imlib_context_set_image(img);
    int w = imlib_image_get_width();
    int h = imlib_image_get_height();
    const uint32_t* data = (const uint32_t*)imlib_image_get_data_for_reading_only();
    glGenTextures(1, &_art_tex);
    glBindTexture(GL_TEXTURE_2D, _art_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, data);
    imlib_free_image();
}

static std::string fmt_time(int64_t us) {
    if (us < 0) us = 0;
    int total_s = (int)(us / 1000000);
    int m = total_s / 60, s = total_s % 60;
    char buf[16]; snprintf(buf, sizeof(buf), "%d:%02d", m, s);
    return buf;
}

static std::string fit_text(Display* dpy, XftFont* font,
                             const std::string& text, int max_px) {
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, font, (FcChar8*)text.c_str(), (int)text.size(), &ext);
    if ((int)ext.width <= max_px) return text;
    std::string t = text;
    while (t.size() > 1) {
        t.pop_back();
        std::string ts = t + "\xe2\x80\xa6";
        XftTextExtentsUtf8(dpy, font, (FcChar8*)ts.c_str(), (int)ts.size(), &ext);
        if ((int)ext.width <= max_px) return ts;
    }
    return t;
}

void Compositor::update_panel_texts() {
    if (!_pan_font_title) return;
    const int TW = PAN_W - 128 - 14;

    std::string title  = _mpris.valid && !_mpris.title.empty()
                         ? _mpris.title : "\xd0\x9d\xd0\xb8\xd1\x87\xd0\xb5\xd0\xb3\xd0\xbe \xd0\xbd\xd0\xb5 \xd0\xb8\xd0\xb3\xd1\x80\xd0\xb0\xd0\xb5\xd1\x82";
    std::string artist = _mpris.valid && !_mpris.artist.empty()
                         ? _mpris.artist : "";
    std::string tpos   = _mpris.valid
                         ? fmt_time(_mpris.position_us) + " / " + fmt_time(_mpris.length_us)
                         : "";

    if (title != _pan_last_title) {
        _pan_last_title = title;
        std::string t = fit_text(_dpy, _pan_font_title, title, TW);
        xft_to_tex(t, _pan_font_title, _pan_title_tex, _pan_title_tw, _pan_title_th);
        size_t h = std::hash<std::string>{}(title);
        _disc_r = 0.3f + 0.5f * ((h & 0xFF) / 255.0f);
        _disc_g = 0.2f + 0.5f * (((h >> 8) & 0xFF) / 255.0f);
        _disc_b = 0.4f + 0.5f * (((h >> 16) & 0xFF) / 255.0f);
    }
    if (artist != _pan_last_artist) {
        _pan_last_artist = artist;
        if (!artist.empty()) {
            std::string a = fit_text(_dpy, _pan_font_artist, artist, TW);
            xft_to_tex(a, _pan_font_artist, _pan_artist_tex, _pan_artist_tw, _pan_artist_th);
        }
    }
    if (tpos != _pan_last_time && _pan_font_time) {
        _pan_last_time = tpos;
        if (!tpos.empty())
            xft_to_tex(tpos, _pan_font_time, _pan_time_tex, _pan_time_tw, _pan_time_th);
    }
}

void Compositor::draw_panel_text(GLuint tex, int tw, int th,
                                 int px, int py, int pan_x, int pan_y) {
    if (!tex || tw <= 0 || th <= 0) return;
    int ax = pan_x + px, ay = pan_y + py;
    float x0 = ax        / (float)_rw * 2.0f - 1.0f;
    float x1 = (ax + tw) / (float)_rw * 2.0f - 1.0f;
    float y0 = 1.0f - 2.0f * ay        / (float)_rh;
    float y1 = 1.0f - 2.0f * (ay + th) / (float)_rh;
    float v[24] = {
        x0, y0, 0, 0,  x0, y1, 0, 1,  x1, y1, 1, 1,
        x0, y0, 0, 0,  x1, y1, 1, 1,  x1, y0, 1, 0,
    };
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void draw_rrect(GLuint prog, GLint u_tex, GLint u_col, GLint u_size,
                       GLuint white, GLuint vao, GLuint vbo,
                       int rw, int rh,
                       int ax, int ay, int aw, int ah,
                       float r, float g, float b, float a) {
    float x0 = ax      / (float)rw * 2.0f - 1.0f;
    float x1 = (ax+aw) / (float)rw * 2.0f - 1.0f;
    float y0 = 1.0f - 2.0f * ay       / (float)rh;
    float y1 = 1.0f - 2.0f * (ay+ah)  / (float)rh;
    float v[24] = {
        x0, y0, 0, 0,  x0, y1, 0, 1,  x1, y1, 1, 1,
        x0, y0, 0, 0,  x1, y1, 1, 1,  x1, y0, 1, 0,
    };
    glUseProgram(prog);
    glUniform2f(u_size, (float)aw, (float)ah);
    glUniform4f(u_col, r, g, b, a);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, white);
    glUniform1i(u_tex, 0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Compositor::render_panel(int sw) {
    if (_pan_anim <= 0.01f) return;
    if (!_pan_font_title) return;

    int pan_x = _mx + (sw - PAN_W) / 2;
    int pan_y = _my + BAR_H + 6;
    int vis_h = (int)(PAN_H * _pan_anim);
    if (vis_h < 4) return;

    update_panel_texts();

    {
        int cap_h = std::min(vis_h, PAN_H);
        XImage* ximg = XGetImage(_dpy, _root, pan_x, pan_y + vis_h - cap_h,
                                 PAN_W, cap_h, AllPlanes, ZPixmap);
        if (ximg) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _pan_bg_tex);
            int bpp = ximg->bits_per_pixel / 8;
            glPixelStorei(GL_UNPACK_ROW_LENGTH, ximg->bytes_per_line / bpp);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, PAN_H - cap_h, PAN_W, cap_h,
                            GL_BGRA, GL_UNSIGNED_BYTE, ximg->data);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            XDestroyImage(ximg);
            glBindFramebuffer(GL_FRAMEBUFFER, _pan_blur_fbo);
            glViewport(0, 0, PAN_W, PAN_H);
            glUseProgram(_prog_blur);
            glBindTexture(GL_TEXTURE_2D, _pan_bg_tex);
            glUniform1i(_u_blur_tex, 0);
            glUniform2f(_u_blur_dir, 4.0f / PAN_W, 0.0f);
            float fq[24] = {-1, 1, 0, 1, -1, -1, 0, 0, 1, -1, 1, 0,
                            -1, 1, 0, 1,  1, -1, 1, 0, 1,  1, 1, 1};
            glBindBuffer(GL_ARRAY_BUFFER, _vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(fq), fq);
            glBindVertexArray(_vao); glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, _rw, _rh);
        }
    }

    float px0 = pan_x         / (float)_rw * 2.0f - 1.0f;
    float px1 = (pan_x+PAN_W) / (float)_rw * 2.0f - 1.0f;
    float py0 = 1.0f - 2.0f * pan_y          / (float)_rh;
    float py1 = 1.0f - 2.0f * (pan_y+vis_h)  / (float)_rh;

    float uv_y0 = (float)(PAN_H - vis_h) / PAN_H;
    float uv_y1 = 1.0f;

    float bv[24] = {
        px0, py0, 0, uv_y0,  px0, py1, 0, uv_y1,  px1, py1, 1, uv_y1,
        px0, py0, 0, uv_y0,  px1, py1, 1, uv_y1,  px1, py0, 1, uv_y0,
    };
    glUseProgram(_prog_pill);
    glUniform2f(_u_pill_size, (float)PAN_W, (float)vis_h);
    glUniform4f(_u_pill_color, 1.f, 1.f, 1.f, 1.f);
    glBindTexture(GL_TEXTURE_2D, _pan_blur_tex);
    glUniform1i(_u_pill_tex, 0);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(bv), bv);
    glBindVertexArray(_vao); glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindTexture(GL_TEXTURE_2D, _white_tex);
    glUniform4f(_u_pill_color, 0.05f, 0.05f, 0.08f, 0.78f);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(bv), bv);
    glBindVertexArray(_vao); glDrawArrays(GL_TRIANGLES, 0, 6);

    if (_pan_anim < 0.95f) return;

    const bool has_track = _mpris.valid;
    if (has_track) {
        const int disc_cx = pan_x + 14 + DISC_R;
        const int disc_cy = pan_y + PAN_H / 2;
        float dx0 = (disc_cx - DISC_R) / (float)_rw * 2.f - 1.f;
        float dx1 = (disc_cx + DISC_R) / (float)_rw * 2.f - 1.f;
        float dy0 = 1.f - 2.f * (disc_cy - DISC_R) / (float)_rh;
        float dy1 = 1.f - 2.f * (disc_cy + DISC_R) / (float)_rh;
        float dv[24] = {
            dx0, dy0, 0, 0,  dx0, dy1, 0, 1,  dx1, dy1, 1, 1,
            dx0, dy0, 0, 0,  dx1, dy1, 1, 1,  dx1, dy0, 1, 0,
        };
        glUseProgram(_prog_disc);
        glUniform3f(_u_disc_color, _disc_r, _disc_g, _disc_b);
        glUniform1f(_u_disc_angle, _disc_angle);
        glUniform1i(_u_disc_has_art, _art_tex ? 1 : 0);
        glUniform1i(_u_disc_art, 1);
        if (_art_tex) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, _art_tex);
            glActiveTexture(GL_TEXTURE0);
        }
        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(dv), dv);
        glBindVertexArray(_vao); glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    const int tx0_px   = has_track ? 128 : (PAN_W - _pan_title_tw) / 2;
    const int ty_title  = has_track ? 22 : (PAN_H - _pan_title_th) / 2;
    const int ty_artist = 48;
    const int ty_bar    = 74, ty_time = 88, ty_ctrl = 116;

    glUseProgram(_prog_text);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(_u_text_tex, 0);

    glUniform4f(_u_text_color, 1.f, 1.f, 1.f, 1.f);
    glBindTexture(GL_TEXTURE_2D, _pan_title_tex);
    draw_panel_text(_pan_title_tex, _pan_title_tw, _pan_title_th,
                    tx0_px, ty_title, pan_x, pan_y);

    if (_pan_artist_tex && !_pan_last_artist.empty()) {
        glUniform4f(_u_text_color, 0.7f, 0.7f, 0.75f, 1.f);
        glBindTexture(GL_TEXTURE_2D, _pan_artist_tex);
        draw_panel_text(_pan_artist_tex, _pan_artist_tw, _pan_artist_th,
                        tx0_px, ty_artist, pan_x, pan_y);
    }

    if (_pan_time_tex && !_pan_last_time.empty()) {
        glUniform4f(_u_text_color, 0.55f, 0.55f, 0.60f, 1.f);
        glBindTexture(GL_TEXTURE_2D, _pan_time_tex);
        draw_panel_text(_pan_time_tex, _pan_time_tw, _pan_time_th,
                        tx0_px, ty_time, pan_x, pan_y);
    }

    if (!has_track) goto done_panel;

    if (_mpris.length_us > 0) {
        const int bar_w = PAN_W - tx0_px - 14;
        const int bar_h = 4;
        draw_rrect(_prog_pill, _u_pill_tex, _u_pill_color, _u_pill_size,
                   _white_tex, _vao, _vbo, _rw, _rh,
                   pan_x + tx0_px, pan_y + ty_bar, bar_w, bar_h,
                   0.4f, 0.4f, 0.44f, 0.6f);
        float frac = (float)std::min(_mpris.position_us, _mpris.length_us) /
                     _mpris.length_us;
        int fill_w = std::max(bar_h, (int)(bar_w * frac));
        draw_rrect(_prog_pill, _u_pill_tex, _u_pill_color, _u_pill_size,
                   _white_tex, _vao, _vbo, _rw, _rh,
                   pan_x + tx0_px, pan_y + ty_bar, fill_w, bar_h,
                   1.f, 1.f, 1.f, 0.9f);
    }

    {
    const float cy  = (float)(pan_y + ty_ctrl);
    const float btn_r = 14.f;
    const float cx  = (float)(pan_x + tx0_px) + (float)(PAN_W - tx0_px - 14) * 0.5f;
    const float gap = btn_r * 2.f + 10.f;
    const float pcx = cx - gap;
    const float ncx = cx + gap;

    draw_rrect(_prog_pill, _u_pill_tex, _u_pill_color, _u_pill_size,
               _white_tex, _vao, _vbo, _rw, _rh,
               (int)(cx - btn_r), (int)(cy - btn_r), (int)(btn_r * 2), (int)(btn_r * 2),
               1.f, 1.f, 1.f, 0.18f);
    draw_rrect(_prog_pill, _u_pill_tex, _u_pill_color, _u_pill_size,
               _white_tex, _vao, _vbo, _rw, _rh,
               (int)(pcx - 12.f), (int)(cy - 12.f), 24, 24,
               1.f, 1.f, 1.f, 0.13f);
    draw_rrect(_prog_pill, _u_pill_tex, _u_pill_color, _u_pill_size,
               _white_tex, _vao, _vbo, _rw, _rh,
               (int)(ncx - 12.f), (int)(cy - 12.f), 24, 24,
               1.f, 1.f, 1.f, 0.13f);

    if (_mpris.playing) {
        draw_solid_rect(cx - 6.f, cy - 9.f, cx - 2.f, cy + 9.f, 1, 1, 1, 1);
        draw_solid_rect(cx + 2.f, cy - 9.f, cx + 6.f, cy + 9.f, 1, 1, 1, 1);
    } else {
        draw_solid_tri(cx - 6.f, cy - 9.f, cx - 6.f, cy + 9.f, cx + 9.f, cy, 1, 1, 1, 1);
    }

    draw_solid_rect(pcx - 8.f, cy - 8.f, pcx - 5.f, cy + 8.f, .85f, .85f, .85f, 1);
    draw_solid_tri(pcx + 7.f, cy - 8.f, pcx + 7.f, cy + 8.f, pcx - 4.f, cy, .85f, .85f, .85f, 1);

    draw_solid_tri(ncx - 7.f, cy - 8.f, ncx - 7.f, cy + 8.f, ncx + 4.f, cy, .85f, .85f, .85f, 1);
    draw_solid_rect(ncx + 5.f, cy - 8.f, ncx + 8.f, cy + 8.f, .85f, .85f, .85f, 1);
    }

done_panel:
    (void)0;
}

bool Compositor::tick(double dt) {
    if (!_ok) return false;

    bool needs_render = false;

    const float SPD = 4.0f;
    if (_pan_open) {
        if (_pan_anim < 1.0f) {
            _pan_anim = std::min(1.0f, _pan_anim + (float)dt * SPD);
            needs_render = true;
        }
    } else {
        if (_pan_anim > 0.0f) {
            _pan_anim = std::max(0.0f, _pan_anim - (float)dt * SPD);
            needs_render = true;
        }
    }

    if (_pan_anim > 0.0f && _mpris.playing) {
        _disc_angle += (float)(dt * 2.0 * 3.14159265 / 8.0);
        needs_render = true;
    }

    if (_pan_anim > 0.0f || _pan_open) {
        _mpris_poll_t += dt;
        if (_mpris_poll_t >= 3.0) {
            _mpris_poll_t = 0.0;
            if (_dbus) {
                MprisInfo info;
                if (mpris_poll(_dbus, info)) {
                    _mpris = info;
                    load_art(_mpris.art_url);
                } else {
                    _mpris = {};
                    load_art("");
                }
                needs_render = true;
            }
        }
    } else {
        _mpris_poll_t = 0.0;
    }

    return needs_render;
}

void Compositor::handle_click(int root_x, int root_y) {
    if (!_ok) return;
    int lx = root_x - _mx, ly = root_y - _my;

    int pill_x_l = (_sw - _bar_pw) / 2;
    int pill_x_r = pill_x_l + _bar_pw;
    int pill_y_t = 4;
    int pill_y_b = 4 + _bar_ph;
    if (lx >= pill_x_l && lx <= pill_x_r && ly >= pill_y_t && ly <= pill_y_b) {
        Atom ta = XInternAtom(_dpy, "_NEMAC_PANEL_TOGGLE", False);
        long val = 0;
        Atom actual; int fmt; unsigned long n, after; unsigned char* data = nullptr;
        if (XGetWindowProperty(_dpy, _root, ta, 0, 1, False,
                XA_CARDINAL, &actual, &fmt, &n, &after, &data) == Success && data) {
            val = *(long*)data;
            XFree(data);
        }
        val++;
        XChangeProperty(_dpy, _root, ta, XA_CARDINAL, 32, PropModeReplace,
                        (unsigned char*)&val, 1);
        XFlush(_dpy);
        return;
    }

    if (!_pan_open || _pan_anim < 0.9f) return;

    int pan_x = _mx + (_sw - PAN_W) / 2;
    int pan_y = _my + BAR_H + 6;

    const int tx0_px = 128;
    const int ctrl_cx_l = pan_x + tx0_px + (PAN_W - tx0_px - 14) / 2;
    const int ctrl_cy   = pan_y + 116;
    const int gap_px    = 14 * 2 + 10;

    int bar_x0 = pan_x + tx0_px;
    int bar_x1 = pan_x + PAN_W - 14;
    int bar_y0 = pan_y + 70;
    int bar_y1 = pan_y + 82;
    if (root_x >= bar_x0 && root_x <= bar_x1 &&
        root_y >= bar_y0 && root_y <= bar_y1 &&
        _mpris.valid && _mpris.length_us > 0 && _dbus) {
        float frac = (float)(root_x - bar_x0) / (bar_x1 - bar_x0);
        int64_t pos = (int64_t)(frac * _mpris.length_us);
        mpris_seek_abs(_dbus, _mpris.player_bus, pos);
        _mpris.position_us = pos;
        return;
    }

    int dx = root_x - ctrl_cx_l, dy = root_y - ctrl_cy;
    if (dx * dx + dy * dy <= 14 * 14 && _mpris.valid && _dbus) {
        mpris_play_pause(_dbus, _mpris.player_bus);
        _mpris.playing = !_mpris.playing;
        return;
    }
    dx = root_x - (ctrl_cx_l - gap_px); dy = root_y - ctrl_cy;
    if (dx * dx + dy * dy <= 10 * 10 && _mpris.valid && _dbus) {
        mpris_prev(_dbus, _mpris.player_bus);
        return;
    }
    dx = root_x - (ctrl_cx_l + gap_px); dy = root_y - ctrl_cy;
    if (dx * dx + dy * dy <= 10 * 10 && _mpris.valid && _dbus) {
        mpris_next(_dbus, _mpris.player_bus);
        return;
    }

    if (root_x < pan_x || root_x > pan_x + PAN_W ||
        root_y < pan_y || root_y > pan_y + PAN_H) {
        _pan_open = false;
        update_overlay_input();
    }
}

void Compositor::update_overlay_input() {
    if (_overlay == None) return;

    int pill_x = _mx + (_sw - _bar_pw) / 2;
    int pill_y_root = _my + 4;

    std::vector<XRectangle> rects;

    { XRectangle r; r.x = (short)_mx; r.y = (short)_my;
      r.width = (unsigned short)_sw; r.height = (unsigned short)BAR_H;
      rects.push_back(r); }

    if (_pan_open) {
        int pan_x = _mx + (_sw - PAN_W) / 2;
        int pan_y = _my + BAR_H + 6;
        XRectangle r; r.x = (short)pan_x; r.y = (short)pan_y;
        r.width = (unsigned short)PAN_W; r.height = (unsigned short)PAN_H;
        rects.push_back(r);
    }

    XserverRegion brgn = XFixesCreateRegion(_dpy, rects.data(), (int)rects.size());
    XFixesSetWindowShapeRegion(_dpy, _overlay, ShapeBounding, 0, 0, brgn);
    XFixesDestroyRegion(_dpy, brgn);

    std::vector<XRectangle> input_rects;
    { XRectangle r; r.x = (short)pill_x; r.y = (short)pill_y_root;
      r.width = (unsigned short)_bar_pw; r.height = (unsigned short)_bar_ph;
      input_rects.push_back(r); }

    if (_pan_open) {
        int pan_x = _mx + (_sw - PAN_W) / 2;
        int pan_y = _my + BAR_H + 6;
        XRectangle r; r.x = (short)pan_x; r.y = (short)pan_y;
        r.width = (unsigned short)PAN_W; r.height = (unsigned short)PAN_H;
        input_rects.push_back(r);
    }

    XserverRegion irgn = XFixesCreateRegion(_dpy, input_rects.data(), (int)input_rects.size());
    XFixesSetWindowShapeRegion(_dpy, _overlay, ShapeInput, 0, 0, irgn);
    XFixesDestroyRegion(_dpy, irgn);
}
