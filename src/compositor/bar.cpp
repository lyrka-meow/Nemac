#define GLEW_STATIC
#include "compositor.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

void Compositor::init_bar() {
    fprintf(stderr, "nemac: инициализация бара sw=%d\n", _sw);
    fflush(stderr);
    _bar_font = XftFontOpenName(_dpy, _screen, "sans:bold:size=13");
    if (!_bar_font) _bar_font = XftFontOpenName(_dpy, _screen, "monospace:size=12");
    if (!_bar_font) { fprintf(stderr, "nemac: бар: нет шрифта\n"); return; }

    XGlyphInfo ext;
    XftTextExtents8(_dpy, _bar_font, (FcChar8*)"00:00", 5, &ext);
    _bar_pw = ext.width + _bar_ph + 24;
    _bar_tw = _bar_pw;

    _bar_pm  = XCreatePixmap(_dpy, _root, _bar_tw, _bar_ph,
                             DefaultDepth(_dpy, _screen));
    _bar_gc  = XCreateGC(_dpy, _bar_pm, 0, nullptr);
    _bar_xdraw = XftDrawCreate(_dpy, _bar_pm,
                               DefaultVisual(_dpy, _screen),
                               DefaultColormap(_dpy, _screen));

    glGenTextures(1, &_bar_time_tex);
    glBindTexture(GL_TEXTURE_2D, _bar_time_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, _bar_tw, _bar_ph,
                 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &_bar_bg_tex);
    glBindTexture(GL_TEXTURE_2D, _bar_bg_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _bar_pw, _bar_ph,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    make_fbo(_bar_pw, _bar_ph, _bar_blur_fbo, _bar_blur_tex);
}

void Compositor::update_bar_time() {
    if (!_bar_font || !_bar_pm) return;
    time_t t = time(nullptr);
    char str[16];
    strftime(str, sizeof(str), "%H:%M", localtime(&t));
    if (strcmp(str, _bar_last_time) == 0) return;
    memcpy(_bar_last_time, str, sizeof(_bar_last_time));

    XSetForeground(_dpy, _bar_gc, 0x000000);
    XFillRectangle(_dpy, _bar_pm, _bar_gc, 0, 0, _bar_tw, _bar_ph);

    XftColor white;
    white.pixel = WhitePixel(_dpy, _screen);
    white.color = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    XGlyphInfo ext;
    XftTextExtents8(_dpy, _bar_font, (FcChar8*)str, (int)strlen(str), &ext);
    int tx = (_bar_tw - ext.width) / 2;
    int ty = (_bar_ph + _bar_font->ascent - _bar_font->descent) / 2;
    XftDrawString8(_bar_xdraw, &white, _bar_font, tx, ty,
                   (FcChar8*)str, (int)strlen(str));

    XImage* img = XGetImage(_dpy, _bar_pm, 0, 0, _bar_tw, _bar_ph,
                            AllPlanes, ZPixmap);
    if (!img) return;
    std::vector<uint8_t> rbuf(_bar_tw * _bar_ph);
    for (int y = 0; y < _bar_ph; y++)
        for (int x = 0; x < _bar_tw; x++)
            rbuf[y * _bar_tw + x] = (uint8_t)((XGetPixel(img, x, y) >> 16) & 0xFF);
    XDestroyImage(img);

    glBindTexture(GL_TEXTURE_2D, _bar_time_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _bar_tw, _bar_ph,
                    GL_RED, GL_UNSIGNED_BYTE, rbuf.data());
}

void Compositor::render_bar(int sw) {
    if (!_bar_font || !_bar_time_tex || _bar_pw == 0) return;
    update_bar_time();

    const int margin_top = 4;
    int pill_x = _mx + (sw - _bar_pw) / 2;
    int pill_y = margin_top;

    float px0 = (float)(pill_x)           / _rw * 2.0f - 1.0f;
    float px1 = (float)(pill_x + _bar_pw) / _rw * 2.0f - 1.0f;
    float py0 = 1.0f - 2.0f * (_my + pill_y)            / (float)_rh;
    float py1 = 1.0f - 2.0f * (_my + pill_y + _bar_ph)  / (float)_rh;

    {
        XImage* ximg = XGetImage(_dpy, _root, pill_x, _my + pill_y,
                                 _bar_pw, _bar_ph, AllPlanes, ZPixmap);
        if (ximg) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _bar_bg_tex);
            int bpp = ximg->bits_per_pixel / 8;
            glPixelStorei(GL_UNPACK_ROW_LENGTH, ximg->bytes_per_line / bpp);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _bar_pw, _bar_ph,
                            GL_BGRA, GL_UNSIGNED_BYTE, ximg->data);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            XDestroyImage(ximg);
            glBindFramebuffer(GL_FRAMEBUFFER, _bar_blur_fbo);
            glViewport(0, 0, _bar_pw, _bar_ph);
            glUseProgram(_prog_blur);
            glBindTexture(GL_TEXTURE_2D, _bar_bg_tex);
            glUniform1i(_u_blur_tex, 0);
            glUniform2f(_u_blur_dir, 4.0f / _bar_pw, 0.0f);
            float fq[24] = {-1, 1, 0, 1, -1, -1, 0, 0, 1, -1, 1, 0,
                            -1, 1, 0, 1,  1, -1, 1, 0, 1,  1, 1, 1};
            glBindBuffer(GL_ARRAY_BUFFER, _vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(fq), fq);
            glBindVertexArray(_vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, _rw, _rh);
        }
    }

    auto pill_quad = [&](float v[24]) {
        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 24 * sizeof(float), v);
        glBindVertexArray(_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };
    auto make_quad = [&](float* v, float u0, float u1) {
        v[0]=px0;v[1]=py0;v[2]=u0;v[3]=0.f;
        v[4]=px0;v[5]=py1;v[6]=u0;v[7]=1.f;
        v[8]=px1;v[9]=py1;v[10]=u1;v[11]=1.f;
        v[12]=px0;v[13]=py0;v[14]=u0;v[15]=0.f;
        v[16]=px1;v[17]=py1;v[18]=u1;v[19]=1.f;
        v[20]=px1;v[21]=py0;v[22]=u1;v[23]=0.f;
    };

    glUseProgram(_prog_pill);
    glUniform2f(_u_pill_size, (float)_bar_pw, (float)_bar_ph);
    glUniform4f(_u_pill_color, 1.f, 1.f, 1.f, 1.f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _bar_blur_tex);
    glUniform1i(_u_pill_tex, 0);
    float bv[24]; make_quad(bv, 0.f, 1.f); pill_quad(bv);

    glBindTexture(GL_TEXTURE_2D, _white_tex);
    glUniform4f(_u_pill_color, 0.05f, 0.05f, 0.08f, 0.50f);
    float tv[24]; make_quad(tv, 0.f, 1.f); pill_quad(tv);

    int tx_left = pill_x + (_bar_pw - _bar_tw) / 2;
    float tx0 = (float)tx_left           / _rw * 2.0f - 1.0f;
    float tx1 = (float)(tx_left + _bar_tw) / _rw * 2.0f - 1.0f;
    float ttv[24] = {
        tx0, py0, 0.f, 0.f, tx0, py1, 0.f, 1.f, tx1, py1, 1.f, 1.f,
        tx0, py0, 0.f, 0.f, tx1, py1, 1.f, 1.f, tx1, py0, 1.f, 0.f,
    };
    glUseProgram(_prog_text);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _bar_time_tex);
    glUniform1i(_u_text_tex, 0);
    glUniform4f(_u_text_color, 1.f, 1.f, 1.f, 1.f);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(ttv), ttv);
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

}
