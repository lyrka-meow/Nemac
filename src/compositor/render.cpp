#define GLEW_STATIC
#include "compositor.h"

void Compositor::render(std::vector<WinInfo>& wins,
                        int view_x, int view_y)
{
    if (!_ok) return;
    _frame_dirty = false;

    glXMakeCurrent(_dpy, _glxwin, _ctx);
    glViewport(0, 0, _rw, _rh);

    int top_dock_end = 0;
    for (auto& w : wins)
        if (w.dock && w.y == 0)
            top_dock_end = std::max(top_dock_end, w.y + w.h);

    auto draw_rect = [&](int rx, int ry, int rw, int rh) {
        float bx0 = (float)rx       / _rw * 2.f - 1.f;
        float bx1 = (float)(rx+rw)  / _rw * 2.f - 1.f;
        float by0 = -(float)ry      / _rh * 2.f + 1.f;
        float by1 = -(float)(ry+rh) / _rh * 2.f + 1.f;
        float sv[24] = {bx0, by0, 0, 0, bx0, by1, 0, 1, bx1, by1, 1, 1,
                        bx0, by0, 0, 0, bx1, by1, 1, 1, bx1, by0, 1, 0};
        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(sv), sv);
        glBindVertexArray(_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };

    auto draw_shadow = [&](int sx, int sy, int ww, int wh) {
        draw_rect(sx + 5, sy + 5, ww, wh);
    };

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    capture_wallpaper();
    if (_wall_ok) {
        glDisable(GL_BLEND);
        glUseProgram(_prog_comp);
        glUniform1i(_u_comp_opaque, 1);
        draw_quad(0, 0, _rw, _rh, _wall_tex, 1.0f);
        glEnable(GL_BLEND);
    } else {
        glClearColor(0.07f, 0.07f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    for (auto& w : wins)
        if (w.dirty && !w.bypass) update_texture(w);

    for (auto& w : wins) {
        if (!w.texture || w.bypass || w.dock) continue;
        int sx = w.x - view_x;
        int sy = w.y - view_y;
        bool bordered = !w.unmanaged && !w.fullscreen && !w.desktop;

        if (bordered) {
            glUseProgram(_prog_shadow);
            glUniform3f(_u_shad_color, 0.0f, 0.0f, 0.0f);
            glUniform1f(_u_shad_alpha, 0.30f);
            if (sy + 5 >= top_dock_end)
                draw_shadow(sx, sy, w.w, w.h);

            if (w.focused)
                glUniform3f(_u_shad_color, 0.533f, 0.667f, 1.0f);
            else
                glUniform3f(_u_shad_color, 0.267f, 0.267f, 0.267f);
            glUniform1f(_u_shad_alpha, 1.0f);
            int border_y = sy - 2;
            int border_h = w.h + 4;
            if (border_y < top_dock_end) {
                int cut = top_dock_end - border_y;
                border_y += cut;
                border_h -= cut;
            }
            if (border_h > 0)
                draw_rect(sx - 2, border_y, w.w + 4, border_h);
        }

        glUseProgram(_prog_comp);
        glUniform1i(_u_comp_opaque, 1);
        draw_quad(sx, sy, w.w, w.h, w.texture, w.opacity);
    }

    for (auto& w : wins) {
        if (!w.dock || !w.texture) continue;
        blur_region(w.x, w.y, w.w, w.h);

        glUseProgram(_prog_shadow);
        glUniform3f(_u_shad_color, 0.18f, 0.18f, 0.24f);
        glUniform1f(_u_shad_alpha, 0.30f);
        draw_rect(w.x, w.y, w.w, w.h);

        glUniform3f(_u_shad_color, 0.0f, 0.0f, 0.0f);
        glUniform1f(_u_shad_alpha, 0.12f);
        draw_rect(w.x, w.y + w.h - 1, w.w, 1);

        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
        glUseProgram(_prog_comp);
        glUniform1i(_u_comp_opaque, 1);
        draw_quad(w.x, w.y, w.w, w.h, w.texture, 1.0f);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                            GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }

    glXSwapBuffers(_dpy, _glxwin);
}
