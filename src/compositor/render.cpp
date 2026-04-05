#define GLEW_STATIC
#include "compositor.h"
#include <sys/time.h>

void Compositor::render(std::vector<WinInfo>& wins,
                        int view_x, int view_y,
                        int sw)
{
    if (!_ok) return;

    struct timeval _rnow; gettimeofday(&_rnow, nullptr);
    double _rt = _rnow.tv_sec + _rnow.tv_usec * 1e-6;
    if (_rt - _last_render_t < 0.033) return;
    _last_render_t = _rt;

    glXMakeCurrent(_dpy, _glxwin, _ctx);
    glViewport(0, 0, _rw, _rh);

    auto draw_shadow = [&](int sx, int sy, int ww, int wh) {
        const int off = 5;
        float bx0 = (float)(sx + off)      / _rw * 2.f - 1.f;
        float bx1 = (float)(sx + off + ww) / _rw * 2.f - 1.f;
        float by0 = -(float)(sy + off)      / _rh * 2.f + 1.f;
        float by1 = -(float)(sy + off + wh) / _rh * 2.f + 1.f;
        float sv[24] = {bx0, by0, 0, 0, bx0, by1, 0, 1, bx1, by1, 1, 1,
                        bx0, by0, 0, 0, bx1, by1, 1, 1, bx1, by0, 1, 0};
        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(sv), sv);
        glBindVertexArray(_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (_nvidia) {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        (void)wins;
    } else {
        glClearColor(0.07f, 0.07f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        for (auto& w : wins)
            if (w.dirty && !w.bypass) update_texture(w);
        for (auto& w : wins) {
            if (!w.texture || w.bypass) continue;
            int sx = w.x - view_x;
            int sy = w.y - view_y;
            if (!w.unmanaged) {
                glUseProgram(_prog_shadow);
                glUniform1f(_u_shad_alpha, 0.30f);
                draw_shadow(sx, sy, w.w, w.h);
            }
            glUseProgram(_prog_comp);
            draw_quad(sx, sy, w.w, w.h, w.texture, w.opacity);
        }
    }

    render_bar(sw);
    render_panel(sw);
    glXSwapBuffers(_dpy, _glxwin);
}
