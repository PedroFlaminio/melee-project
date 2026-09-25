#include "render/sdl_gl_renderer.hpp"

#include "custom_textures.hpp"
#include "gx/tev.hpp"
#include "gx/view.hpp"
#include "render/play_window.hpp"
#include "render/settings_ui.hpp"
#include <melee_host/gx.h>
#include <melee_host/host.h>
#include <melee_host/input.h>
#include <melee_host/video.h>
#include <melee_host/boot.h>

#define GL_GLEXT_PROTOTYPES 1
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <utility>

#include <dolphin/pad.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

namespace melee::render {

    static int g_anisotropy_level = 0;
    namespace {

#if defined(_WIN32)
#define MELEE_GL_FUNCTION(type, name) static type melee_##name = nullptr
        MELEE_GL_FUNCTION(PFNGLACTIVETEXTUREPROC, glActiveTexture);
        MELEE_GL_FUNCTION(PFNGLATTACHSHADERPROC, glAttachShader);
        MELEE_GL_FUNCTION(PFNGLBINDBUFFERPROC, glBindBuffer);
        MELEE_GL_FUNCTION(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer);
        MELEE_GL_FUNCTION(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer);
        MELEE_GL_FUNCTION(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray);
        MELEE_GL_FUNCTION(PFNGLBLENDEQUATIONPROC, glBlendEquation);
        MELEE_GL_FUNCTION(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer);
        MELEE_GL_FUNCTION(PFNGLBUFFERDATAPROC, glBufferData);
        MELEE_GL_FUNCTION(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus);
        MELEE_GL_FUNCTION(PFNGLCOMPILESHADERPROC, glCompileShader);
        MELEE_GL_FUNCTION(PFNGLCREATEPROGRAMPROC, glCreateProgram);
        MELEE_GL_FUNCTION(PFNGLCREATESHADERPROC, glCreateShader);
        MELEE_GL_FUNCTION(PFNGLDELETEBUFFERSPROC, glDeleteBuffers);
        MELEE_GL_FUNCTION(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers);
        MELEE_GL_FUNCTION(PFNGLDELETEPROGRAMPROC, glDeleteProgram);
        MELEE_GL_FUNCTION(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers);
        MELEE_GL_FUNCTION(PFNGLDELETESHADERPROC, glDeleteShader);
        MELEE_GL_FUNCTION(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays);
        MELEE_GL_FUNCTION(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray);
        MELEE_GL_FUNCTION(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer);
        MELEE_GL_FUNCTION(PFNGLGENBUFFERSPROC, glGenBuffers);
        MELEE_GL_FUNCTION(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap);
        MELEE_GL_FUNCTION(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers);
        MELEE_GL_FUNCTION(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers);
        MELEE_GL_FUNCTION(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays);
        MELEE_GL_FUNCTION(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog);
        MELEE_GL_FUNCTION(PFNGLGETPROGRAMIVPROC, glGetProgramiv);
        MELEE_GL_FUNCTION(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog);
        MELEE_GL_FUNCTION(PFNGLGETSHADERIVPROC, glGetShaderiv);
        MELEE_GL_FUNCTION(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation);
        MELEE_GL_FUNCTION(PFNGLLINKPROGRAMPROC, glLinkProgram);
        MELEE_GL_FUNCTION(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage);
        MELEE_GL_FUNCTION(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC, glRenderbufferStorageMultisample);
        MELEE_GL_FUNCTION(PFNGLSHADERSOURCEPROC, glShaderSource);
        MELEE_GL_FUNCTION(PFNGLUNIFORM1IPROC, glUniform1i);
        MELEE_GL_FUNCTION(PFNGLUNIFORM1IVPROC, glUniform1iv);
        MELEE_GL_FUNCTION(PFNGLUNIFORM2FPROC, glUniform2f);
        MELEE_GL_FUNCTION(PFNGLUNIFORM3FVPROC, glUniform3fv);
        MELEE_GL_FUNCTION(PFNGLUNIFORM4IPROC, glUniform4i);
        MELEE_GL_FUNCTION(PFNGLUNIFORM4IVPROC, glUniform4iv);
        MELEE_GL_FUNCTION(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv);
        MELEE_GL_FUNCTION(PFNGLUSEPROGRAMPROC, glUseProgram);
        MELEE_GL_FUNCTION(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer);
#undef MELEE_GL_FUNCTION

        bool load_modern_gl_functions(std::string* error)
        {
#define MELEE_LOAD_GL(type, name)                                                \
    melee_##name = reinterpret_cast<type>(SDL_GL_GetProcAddress(#name));         \
    if (melee_##name == nullptr) {                                                \
        *error = std::string("OpenGL function unavailable: ") + #name;          \
        return false;                                                             \
    }
            MELEE_LOAD_GL(PFNGLACTIVETEXTUREPROC, glActiveTexture)
            MELEE_LOAD_GL(PFNGLATTACHSHADERPROC, glAttachShader)
            MELEE_LOAD_GL(PFNGLBINDBUFFERPROC, glBindBuffer)
            MELEE_LOAD_GL(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer)
            MELEE_LOAD_GL(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer)
            MELEE_LOAD_GL(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)
            MELEE_LOAD_GL(PFNGLBLENDEQUATIONPROC, glBlendEquation)
            MELEE_LOAD_GL(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer)
            MELEE_LOAD_GL(PFNGLBUFFERDATAPROC, glBufferData)
            MELEE_LOAD_GL(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus)
            MELEE_LOAD_GL(PFNGLCOMPILESHADERPROC, glCompileShader)
            MELEE_LOAD_GL(PFNGLCREATEPROGRAMPROC, glCreateProgram)
            MELEE_LOAD_GL(PFNGLCREATESHADERPROC, glCreateShader)
            MELEE_LOAD_GL(PFNGLDELETEBUFFERSPROC, glDeleteBuffers)
            MELEE_LOAD_GL(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers)
            MELEE_LOAD_GL(PFNGLDELETEPROGRAMPROC, glDeleteProgram)
            MELEE_LOAD_GL(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers)
            MELEE_LOAD_GL(PFNGLDELETESHADERPROC, glDeleteShader)
            MELEE_LOAD_GL(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)
            MELEE_LOAD_GL(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)
            MELEE_LOAD_GL(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer)
            MELEE_LOAD_GL(PFNGLGENBUFFERSPROC, glGenBuffers)
            MELEE_LOAD_GL(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap)
            MELEE_LOAD_GL(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers)
            MELEE_LOAD_GL(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers)
            MELEE_LOAD_GL(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)
            MELEE_LOAD_GL(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)
            MELEE_LOAD_GL(PFNGLGETPROGRAMIVPROC, glGetProgramiv)
            MELEE_LOAD_GL(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)
            MELEE_LOAD_GL(PFNGLGETSHADERIVPROC, glGetShaderiv)
            MELEE_LOAD_GL(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation)
            MELEE_LOAD_GL(PFNGLLINKPROGRAMPROC, glLinkProgram)
            MELEE_LOAD_GL(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage)
            MELEE_LOAD_GL(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC, glRenderbufferStorageMultisample)
            MELEE_LOAD_GL(PFNGLSHADERSOURCEPROC, glShaderSource)
            MELEE_LOAD_GL(PFNGLUNIFORM1IPROC, glUniform1i)
            MELEE_LOAD_GL(PFNGLUNIFORM1IVPROC, glUniform1iv)
            MELEE_LOAD_GL(PFNGLUNIFORM2FPROC, glUniform2f)
            MELEE_LOAD_GL(PFNGLUNIFORM3FVPROC, glUniform3fv)
            MELEE_LOAD_GL(PFNGLUNIFORM4IPROC, glUniform4i)
            MELEE_LOAD_GL(PFNGLUNIFORM4IVPROC, glUniform4iv)
            MELEE_LOAD_GL(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv)
            MELEE_LOAD_GL(PFNGLUSEPROGRAMPROC, glUseProgram)
            MELEE_LOAD_GL(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)
#undef MELEE_LOAD_GL
            return true;
        }

#define glActiveTexture melee_glActiveTexture
#define glAttachShader melee_glAttachShader
#define glBindBuffer melee_glBindBuffer
#define glBindFramebuffer melee_glBindFramebuffer
#define glBindRenderbuffer melee_glBindRenderbuffer
#define glBindVertexArray melee_glBindVertexArray
#define glBlendEquation melee_glBlendEquation
#define glBlitFramebuffer melee_glBlitFramebuffer
#define glBufferData melee_glBufferData
#define glCheckFramebufferStatus melee_glCheckFramebufferStatus
#define glCompileShader melee_glCompileShader
#define glCreateProgram melee_glCreateProgram
#define glCreateShader melee_glCreateShader
#define glDeleteBuffers melee_glDeleteBuffers
#define glDeleteFramebuffers melee_glDeleteFramebuffers
#define glDeleteProgram melee_glDeleteProgram
#define glDeleteRenderbuffers melee_glDeleteRenderbuffers
#define glDeleteShader melee_glDeleteShader
#define glDeleteVertexArrays melee_glDeleteVertexArrays
#define glEnableVertexAttribArray melee_glEnableVertexAttribArray
#define glFramebufferRenderbuffer melee_glFramebufferRenderbuffer
#define glGenBuffers melee_glGenBuffers
#define glGenerateMipmap melee_glGenerateMipmap
#define glGenFramebuffers melee_glGenFramebuffers
#define glGenRenderbuffers melee_glGenRenderbuffers
#define glGenVertexArrays melee_glGenVertexArrays
#define glGetProgramInfoLog melee_glGetProgramInfoLog
#define glGetProgramiv melee_glGetProgramiv
#define glGetShaderInfoLog melee_glGetShaderInfoLog
#define glGetShaderiv melee_glGetShaderiv
#define glGetUniformLocation melee_glGetUniformLocation
#define glLinkProgram melee_glLinkProgram
#define glRenderbufferStorage melee_glRenderbufferStorage
#define glRenderbufferStorageMultisample melee_glRenderbufferStorageMultisample
#define glShaderSource melee_glShaderSource
#define glUniform1i melee_glUniform1i
#define glUniform1iv melee_glUniform1iv
#define glUniform2f melee_glUniform2f
#define glUniform3fv melee_glUniform3fv
#define glUniform4i melee_glUniform4i
#define glUniform4iv melee_glUniform4iv
#define glUniformMatrix4fv melee_glUniformMatrix4fv
#define glUseProgram melee_glUseProgram
#define glVertexAttribPointer melee_glVertexAttribPointer
#endif

        std::vector<TextureImage> texture_images;

        /* GX's compare functions are enumerated in the same order as OpenGL's,
         * so the mapping is an offset rather than a table. */
        GLenum gl_compare(mh_u32 compare)
        {
            return static_cast<GLenum>(GL_NEVER + (compare & 7U));
        }

        /* A blend factor names the other side's colour depending on which side
         * it is used on: GX gives GX_BL_SRCCLR and GX_BL_DSTCLR the same
         * value, and which one it means follows from whether it multiplies the
         * source or the destination. */
        GLenum gl_blend_factor(mh_u32 factor, bool source_side)
        {
            switch (factor) {
            case 0: // GX_BL_ZERO
                return GL_ZERO;
            case 1: // GX_BL_ONE
                return GL_ONE;
            case 2: // GX_BL_SRCCLR as a destination factor, GX_BL_DSTCLR as a
                    // source
                return source_side ? GL_DST_COLOR : GL_SRC_COLOR;
            case 3:
                return source_side ? GL_ONE_MINUS_DST_COLOR
                                   : GL_ONE_MINUS_SRC_COLOR;
            case 4: // GX_BL_SRCALPHA
                return GL_SRC_ALPHA;
            case 5: // GX_BL_INVSRCALPHA
                return GL_ONE_MINUS_SRC_ALPHA;
            case 6: // GX_BL_DSTALPHA
                return GL_DST_ALPHA;
            case 7: // GX_BL_INVDSTALPHA
                return GL_ONE_MINUS_DST_ALPHA;
            default:
                return source_side ? GL_ONE : GL_ZERO;
            }
        }

        bool is_blended(const MeleeHostGxDrawState& state)
        {
            return state.blend_mode == 1 || state.blend_mode == 3;
        }

        /* Applies the parts of one captured state that are not in the shader.
         * The alpha test is: it runs on the TEV result, which only the shader
         * has. Returns false when the state draws nothing at all, which
         * GX_CULL_ALL does. */
        bool apply_draw_state(const MeleeHostGxDrawState& state,
                              bool front_face_cw)
        {
            glFrontFace(front_face_cw ? GL_CW : GL_CCW);
            switch (state.cull_mode) {
            case 0: // GX_CULL_NONE
                glDisable(GL_CULL_FACE);
                break;
            case 1: // GX_CULL_FRONT
                glEnable(GL_CULL_FACE);
                glCullFace(GL_FRONT);
                break;
            case 2: // GX_CULL_BACK
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                break;
            default: // GX_CULL_ALL
                return false;
            }

            if (state.z_compare_enable) {
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(gl_compare(state.z_func));
            } else {
                glDisable(GL_DEPTH_TEST);
            }
            glDepthMask(state.z_update_enable ? GL_TRUE : GL_FALSE);

            switch (state.blend_mode) {
            case 1: // GX_BM_BLEND
                glEnable(GL_BLEND);
                glBlendEquation(GL_FUNC_ADD);
                glBlendFunc(gl_blend_factor(state.blend_src_factor, true),
                            gl_blend_factor(state.blend_dst_factor, false));
                break;
            case 3: // GX_BM_SUBTRACT, which GX fixes at destination minus
                    // source
                glEnable(GL_BLEND);
                glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
                glBlendFunc(GL_ONE, GL_ONE);
                break;
            default: // GX_BM_NONE, and GX_BM_LOGIC which this viewer does not
                     // model
                glDisable(GL_BLEND);
                break;
            }

            const GLboolean color =
                state.color_update_enable ? GL_TRUE : GL_FALSE;
            const GLboolean alpha =
                state.alpha_update_enable ? GL_TRUE : GL_FALSE;
            glColorMask(color, color, color, alpha);
            return true;
        }

        struct Bounds {
            float minimum[3] = { std::numeric_limits<float>::max(),
                                 std::numeric_limits<float>::max(),
                                 std::numeric_limits<float>::max() };
            float maximum[3] = { std::numeric_limits<float>::lowest(),
                                 std::numeric_limits<float>::lowest(),
                                 std::numeric_limits<float>::lowest() };
        };

        void extend(Bounds* bounds, const MeleeHostGxPosition3f32& position)
        {
            const std::array<float, 3> values{ position.x, position.y,
                                               position.z };
            for (std::size_t axis = 0; axis < values.size(); ++axis) {
                bounds->minimum[axis] =
                    std::min(bounds->minimum[axis], values[axis]);
                bounds->maximum[axis] =
                    std::max(bounds->maximum[axis], values[axis]);
            }
        }

        /* Column-major, as GL reads it. */
        using Mat4 = std::array<float, 16>;

        Mat4 identity()
        {
            return { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        }

        Mat4 multiply(const Mat4& left, const Mat4& right)
        {
            Mat4 result{};
            for (std::size_t column = 0; column < 4; ++column) {
                for (std::size_t row = 0; row < 4; ++row) {
                    float sum = 0.0F;
                    for (std::size_t k = 0; k < 4; ++k) {
                        sum += left[k * 4 + row] * right[column * 4 + k];
                    }
                    result[column * 4 + row] = sum;
                }
            }
            return result;
        }

        Mat4 translation(float x, float y, float z)
        {
            Mat4 result = identity();
            result[12] = x;
            result[13] = y;
            result[14] = z;
            return result;
        }

        Mat4 rotation_x(float degrees)
        {
            const float radians = degrees * 3.14159265F / 180.0F;
            Mat4 result = identity();
            result[5] = std::cos(radians);
            result[6] = std::sin(radians);
            result[9] = -std::sin(radians);
            result[10] = std::cos(radians);
            return result;
        }

        Mat4 rotation_y(float degrees)
        {
            const float radians = degrees * 3.14159265F / 180.0F;
            Mat4 result = identity();
            result[0] = std::cos(radians);
            result[2] = -std::sin(radians);
            result[8] = std::sin(radians);
            result[10] = std::cos(radians);
            return result;
        }

        Mat4 frustum(float left, float right, float bottom, float top,
                     float near_plane, float far_plane)
        {
            Mat4 result{};
            result[0] = 2.0F * near_plane / (right - left);
            result[5] = 2.0F * near_plane / (top - bottom);
            result[8] = (right + left) / (right - left);
            result[9] = (top + bottom) / (top - bottom);
            result[10] = -(far_plane + near_plane) / (far_plane - near_plane);
            result[11] = -1.0F;
            result[14] =
                -2.0F * far_plane * near_plane / (far_plane - near_plane);
            return result;
        }

        /* An orbit camera framing the capture's bounds. */
        Mat4 preview_mvp(const Bounds& bounds, int width, int height,
                         float yaw, float pitch, float zoom)
        {
            const float center_x =
                (bounds.minimum[0] + bounds.maximum[0]) * 0.5F;
            const float center_y =
                (bounds.minimum[1] + bounds.maximum[1]) * 0.5F;
            const float center_z =
                (bounds.minimum[2] + bounds.maximum[2]) * 0.5F;
            const float extent_x = bounds.maximum[0] - bounds.minimum[0];
            const float extent_y = bounds.maximum[1] - bounds.minimum[1];
            const float extent_z = bounds.maximum[2] - bounds.minimum[2];
            const float aspect =
                static_cast<float>(width) / static_cast<float>(height);
            const float radius =
                std::max({ extent_x, extent_y, extent_z, 1.0F }) * 0.7F;
            const float near_plane = std::max(radius * 0.01F, 0.01F);
            const float distance = radius * 3.0F * zoom;
            const float half_height = near_plane * 0.5F;

            const Mat4 projection = frustum(
                -half_height * aspect, half_height * aspect, -half_height,
                half_height, near_plane, distance + radius * 4.0F);
            Mat4 view = translation(0.0F, 0.0F, -distance);
            view = multiply(view, rotation_x(pitch));
            view = multiply(view, rotation_y(yaw));
            view =
                multiply(view, translation(-center_x, -center_y, -center_z));
            return multiply(projection, view);
        }

        GLuint compile_shader(GLenum type, const std::string& source,
                              std::string* log)
        {
            GLuint shader = glCreateShader(type);
            const char* text = source.c_str();
            glShaderSource(shader, 1, &text, nullptr);
            glCompileShader(shader);
            GLint compiled = GL_FALSE;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if (compiled != GL_TRUE) {
                std::array<char, 4096> buffer{};
                glGetShaderInfoLog(shader, static_cast<GLsizei>(buffer.size()),
                                   nullptr, buffer.data());
                *log = buffer.data();
                glDeleteShader(shader);
                return 0;
            }
            return shader;
        }

        struct TevProgram {
            GLuint program = 0;
            GLint mvp = -1;
            GLint registers = -1;
            GLint konst = -1;
            GLint alpha_test = -1;
            GLint alpha_ref1 = -1;
            GLint fog_type = -1;
            GLint fog_range = -1;
            GLint fog_color = -1;
            GLint z_texture_op = -1;
            GLint z_texture_bias = -1;
            GLint texmap_formats = -1;
            GLint indirect_matrices = -1;
            GLint indirect_matrix_scales = -1;
        };

        /* One linked program per distinct shader source.  The source is
         * generated from the program's structure only, so the map stays small:
         * constants are set per draw as uniforms. */
        class ProgramCache {
        public:
            ProgramCache()
            {
                std::string log;
                vertex_shader_ = compile_shader(
                    GL_VERTEX_SHADER, melee::gx::tev_vertex_shader_source(),
                    &log);
                if (vertex_shader_ == 0) {
                    std::cerr << "TEV vertex shader failed: " << log << '\n';
                }
            }

            ProgramCache(const ProgramCache&) = delete;
            ProgramCache& operator=(const ProgramCache&) = delete;

            ~ProgramCache()
            {
                for (const auto& entry : programs_) {
                    glDeleteProgram(entry.second.program);
                }
                if (vertex_shader_ != 0) {
                    glDeleteShader(vertex_shader_);
                }
            }

            const TevProgram* get(const MeleeHostGxTevState& tev,
                                  const MeleeHostGxIndirectState& indirect)
            {
                std::string source =
                    melee::gx::tev_fragment_shader_source(tev, indirect);
                const auto found = programs_.find(source);
                if (found != programs_.end()) {
                    return &found->second;
                }
                if (vertex_shader_ == 0 || failures_.contains(source)) {
                    return nullptr;
                }
                std::string log;
                const GLuint fragment =
                    compile_shader(GL_FRAGMENT_SHADER, source, &log);
                if (fragment == 0) {
                    std::cerr << "TEV fragment shader failed: " << log << '\n'
                              << source << '\n';
                    failures_.insert(std::move(source));
                    return nullptr;
                }
                TevProgram program;
                program.program = glCreateProgram();
                glAttachShader(program.program, vertex_shader_);
                glAttachShader(program.program, fragment);
                glLinkProgram(program.program);
                glDeleteShader(fragment);
                GLint linked = GL_FALSE;
                glGetProgramiv(program.program, GL_LINK_STATUS, &linked);
                if (linked != GL_TRUE) {
                    std::array<char, 4096> buffer{};
                    glGetProgramInfoLog(program.program,
                                        static_cast<GLsizei>(buffer.size()),
                                        nullptr, buffer.data());
                    std::cerr << "TEV program link failed: " << buffer.data()
                              << '\n';
                    glDeleteProgram(program.program);
                    failures_.insert(std::move(source));
                    return nullptr;
                }
                program.mvp = glGetUniformLocation(program.program, "u_mvp");
                program.registers =
                    glGetUniformLocation(program.program, "u_register");
                program.konst =
                    glGetUniformLocation(program.program, "u_konst");
                program.alpha_test =
                    glGetUniformLocation(program.program, "u_alpha_test");
                program.alpha_ref1 =
                    glGetUniformLocation(program.program, "u_alpha_ref1");
                program.fog_type =
                    glGetUniformLocation(program.program, "u_fog_type");
                program.fog_range =
                    glGetUniformLocation(program.program, "u_fog_range");
                program.fog_color =
                    glGetUniformLocation(program.program, "u_fog_color");
                program.z_texture_op =
                    glGetUniformLocation(program.program, "u_z_texture_op");
                program.z_texture_bias =
                    glGetUniformLocation(program.program, "u_z_texture_bias");
                program.texmap_formats = glGetUniformLocation(
                    program.program, "u_texmap_format");
                program.indirect_matrices = glGetUniformLocation(
                    program.program, "u_indirect_matrix");
                program.indirect_matrix_scales = glGetUniformLocation(
                    program.program, "u_indirect_matrix_scale");
                glUseProgram(program.program);
                const GLint samplers =
                    glGetUniformLocation(program.program, "u_texmap");
                const std::array<GLint, MELEE_HOST_GX_MAX_TEXMAP> units{
                    0, 1, 2, 3, 4, 5, 6, 7
                };
                if (samplers >= 0) {
                    glUniform1iv(samplers, static_cast<GLsizei>(units.size()),
                                 units.data());
                }
                return &programs_.emplace(std::move(source), program)
                            .first->second;
            }

        private:
            GLuint vertex_shader_ = 0;
            std::map<std::string, TevProgram> programs_;
            std::set<std::string> failures_;
        };

        void set_program_uniforms(const TevProgram& program, const Mat4& mvp,
                                  const MeleeHostGxTevState& tev,
                                  const MeleeHostGxDrawState& state,
                                  const std::array<GLint,
                                                   MELEE_HOST_GX_MAX_TEXMAP>&
                                      texture_formats)
        {
            glUseProgram(program.program);
            glUniformMatrix4fv(program.mvp, 1, GL_FALSE, mvp.data());
            std::array<GLint, 16> registers{};
            std::array<GLint, 16> konst{};
            for (std::size_t index = 0; index < 4; ++index) {
                for (std::size_t channel = 0; channel < 4; ++channel) {
                    registers[index * 4 + channel] =
                        tev.registers[index][channel];
                    konst[index * 4 + channel] =
                        tev.konst_colors[index][channel];
                }
            }
            glUniform4iv(program.registers, 4, registers.data());
            glUniform4iv(program.konst, 4, konst.data());
            glUniform4i(program.alpha_test,
                        static_cast<GLint>(state.alpha_compare_0),
                        static_cast<GLint>(state.alpha_ref_0),
                        static_cast<GLint>(state.alpha_op),
                        static_cast<GLint>(state.alpha_compare_1));
            glUniform1i(program.alpha_ref1,
                        static_cast<GLint>(state.alpha_ref_1));
            glUniform1i(program.fog_type, static_cast<GLint>(state.fog_type));
            glUniform2f(program.fog_range, state.fog_start_z, state.fog_end_z);
            glUniform4i(program.fog_color,
                        static_cast<GLint>(state.fog_color[0]),
                        static_cast<GLint>(state.fog_color[1]),
                        static_cast<GLint>(state.fog_color[2]),
                        static_cast<GLint>(state.fog_color[3]));
            glUniform1i(program.z_texture_op,
                        static_cast<GLint>(state.z_texture_op));
            glUniform1i(program.z_texture_bias,
                        static_cast<GLint>(state.z_texture_bias));
            glUniform1iv(program.texmap_formats,
                         static_cast<GLsizei>(texture_formats.size()),
                         texture_formats.data());
            std::array<GLfloat, MELEE_HOST_GX_MAX_INDIRECT_MATRIX * 2U * 3U>
                indirect_matrices{};
            std::array<GLint, MELEE_HOST_GX_MAX_INDIRECT_MATRIX>
                indirect_matrix_scales{};
            for (std::size_t matrix = 0;
                 matrix < MELEE_HOST_GX_MAX_INDIRECT_MATRIX; ++matrix)
            {
                indirect_matrix_scales[matrix] =
                    state.indirect.matrices[matrix].scale_exp;
                for (std::size_t row = 0; row < 2; ++row) {
                    for (std::size_t column = 0; column < 3; ++column) {
                        indirect_matrices[(matrix * 2U + row) * 3U + column] =
                            state.indirect.matrices[matrix]
                                .offset[row][column];
                    }
                }
            }
            glUniform3fv(program.indirect_matrices,
                         MELEE_HOST_GX_MAX_INDIRECT_MATRIX * 2,
                         indirect_matrices.data());
            glUniform1iv(program.indirect_matrix_scales,
                         MELEE_HOST_GX_MAX_INDIRECT_MATRIX,
                         indirect_matrix_scales.data());
        }

        /* position, COLOR0A0, COLOR1A1, then eight s/t/q texture coordinates.
         */
        constexpr std::size_t kFloatsPerVertex = 3 + 4 + 4 + 3 * 8 + 3 + 3 + 3;

        /* Vertex layout shared by the preview and the conformance runner. */
        void bind_vertex_layout()
        {
            static constexpr GLsizei kStride =
                static_cast<GLsizei>(kFloatsPerVertex * sizeof(float));
            const auto pointer = [](GLuint location, GLint size,
                                    std::size_t offset) {
                glEnableVertexAttribArray(location);
                glVertexAttribPointer(
                    location, size, GL_FLOAT, GL_FALSE, kStride,
                    reinterpret_cast<const void*>(offset * sizeof(float)));
            };
            pointer(0, 3, 0);
            pointer(1, 4, 3);
            pointer(2, 4, 7);
            for (GLuint coord = 0; coord < MELEE_HOST_GX_MAX_TEXCOORD; ++coord)
            {
                pointer(3 + coord, 3,
                        11 + 3 * static_cast<std::size_t>(coord));
            }
            pointer(11, 3, 11 + 3 * MELEE_HOST_GX_MAX_TEXCOORD);
            pointer(12, 3, 11 + 3 * MELEE_HOST_GX_MAX_TEXCOORD + 3);
            pointer(13, 3, 11 + 3 * MELEE_HOST_GX_MAX_TEXCOORD + 6);
        }

        void append_vertex(std::vector<float>* out,
                           const MeleeHostGxCapturedVertex& vertex)
        {
            out->push_back(vertex.position.x);
            out->push_back(vertex.position.y);
            out->push_back(vertex.position.z);
            for (std::size_t channel = 0; channel < 2; ++channel) {
                for (std::size_t component = 0; component < 4; ++component) {
                    out->push_back(
                        static_cast<float>(
                            vertex.raster_color[channel][component]) /
                        255.0F);
                }
            }
            for (const auto& coord : vertex.texgen) {
                out->push_back(coord[0]);
                out->push_back(coord[1]);
                out->push_back(coord[2]);
            }
            out->push_back(vertex.normal.x);
            out->push_back(vertex.normal.y);
            out->push_back(vertex.normal.z);
            out->push_back(vertex.tangent.x);
            out->push_back(vertex.tangent.y);
            out->push_back(vertex.tangent.z);
            out->push_back(vertex.binormal.x);
            out->push_back(vertex.binormal.y);
            out->push_back(vertex.binormal.z);
        }

        /* A run of consecutive triangles sharing pixel state, TEV program,
         * texture set and view, which is what one GL draw call can cover. */
        struct DrawRun {
            mh_u32 draw_state = 0;
            mh_u32 tev_state = 0;
            mh_u32 texture_set = 0;
            mh_u32 view_state = 0;
            GLint first = 0;
            GLsizei count = 0;
            /* The capture's index of the run's first triangle. */
            std::size_t first_triangle = 0;
            bool blended = false;
        };

        struct Capture {
            std::vector<float> vertices;
            std::vector<DrawRun> runs;
            Bounds bounds;
        };

        /* Reads the capture in draw order.  For the preview, runs that blend
         * move behind the ones that do not, keeping their relative order, so
         * opaque geometry has written depth before a translucent run reads it;
         * the original render passes already draw that way, and this keeps a
         * capture that interleaves them readable.  A frame the game drew keeps
         * its own order, because it layers cameras and passes on purpose. */
        Capture read_capture(bool keep_draw_order = false)
        {
            Capture capture;
            const std::size_t triangle_count = melee_host_gx_triangle_count();
            /* A run does not span an EFB clear the frame asked for between its
             * triangles, so a presenter can clear between runs. */
            std::vector<std::size_t> clear_positions;
            for (std::size_t index = 0;
                 index < melee_host_gx_efb_clear_count(); ++index)
            {
                MeleeHostGxEfbClear clear{};
                if (melee_host_gx_efb_clear_at(index, &clear)) {
                    clear_positions.push_back(clear.triangle);
                }
            }
            capture.vertices.reserve(triangle_count * 3 * kFloatsPerVertex);
            std::vector<DrawRun> runs;
            for (std::size_t index = 0; index < triangle_count; ++index) {
                MeleeHostGxCapturedTriangle triangle{};
                if (!melee_host_gx_captured_triangle_at(index, &triangle)) {
                    continue;
                }
                const MeleeHostGxCapturedVertex& lead = triangle.vertices[0];
                const auto first = static_cast<GLint>(capture.vertices.size() /
                                                      kFloatsPerVertex);
                for (const auto& vertex : triangle.vertices) {
                    extend(&capture.bounds, vertex.position);
                    append_vertex(&capture.vertices, vertex);
                }
                if (!runs.empty() &&
                    runs.back().draw_state == lead.draw_state &&
                    runs.back().tev_state == lead.tev_state &&
                    runs.back().texture_set == lead.texture_set &&
                    runs.back().view_state == lead.view_state &&
                    !std::binary_search(clear_positions.begin(),
                                        clear_positions.end(), index))
                {
                    runs.back().count += 3;
                    continue;
                }
                DrawRun run;
                run.draw_state = lead.draw_state;
                run.tev_state = lead.tev_state;
                run.texture_set = lead.texture_set;
                run.view_state = lead.view_state;
                run.first = first;
                run.first_triangle = index;
                run.count = 3;
                MeleeHostGxDrawState state{};
                run.blended = melee_host_gx_captured_draw_state_at(
                                  lead.draw_state, &state) &&
                              is_blended(state);
                runs.push_back(run);
            }
            if (!keep_draw_order) {
                std::stable_partition(
                    runs.begin(), runs.end(),
                    [](const DrawRun& run) { return !run.blended; });
            }
            capture.runs = std::move(runs);
            return capture;
        }

        GLuint create_texture(const TextureImage& image)
        {
            GLuint texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            
            GLint min_filter = image.linear_filter ? GL_LINEAR : GL_NEAREST;
            if (image.mipmap) {
                min_filter = image.linear_filter ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
            }
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, image.linear_filter ? GL_LINEAR : GL_NEAREST);
            const auto wrap = [](std::uint32_t mode) {
                return mode == 1   ? GL_REPEAT
                       : mode == 2 ? GL_MIRRORED_REPEAT
                                   : GL_CLAMP_TO_EDGE;
            };
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                            wrap(image.wrap_s));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                            wrap(image.wrap_t));
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
            if (image.mipmap) {
                glGenerateMipmap(GL_TEXTURE_2D);
            }
            if (SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
                GLfloat maximum = 1.0F;
                glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maximum);
                const GLfloat requested = g_anisotropy_level > 0
                    ? static_cast<GLfloat>(g_anisotropy_level) : 1.0F;
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                                std::min(requested, maximum));
            }
            return texture;
        }

        /* Renders one run into the current framebuffer, viewport and scissor
         * box. */
        void draw_run(const DrawRun& run, ProgramCache* programs,
                      const std::vector<GLuint>& textures,
                      const std::vector<std::uint32_t>& texture_formats,
                      GLuint white_texture, const Mat4& mvp,
                      bool front_face_cw)
        {
            MeleeHostGxDrawState state{};
            MeleeHostGxTevState tev{};
            if (!melee_host_gx_captured_draw_state_at(run.draw_state,
                                                      &state) ||
                !melee_host_gx_captured_tev_state_at(run.tev_state, &tev) ||
                !apply_draw_state(state, front_face_cw))
            {
                return;
            }
            const TevProgram* const program = programs->get(tev,
                                                              state.indirect);
            if (program == nullptr) {
                return;
            }
            std::array<mh_u32, MELEE_HOST_GX_MAX_TEXMAP> set{};
            set.fill(MELEE_HOST_GX_NO_TEXTURE);
            melee_host_gx_captured_texture_set_at(run.texture_set, set.data());
            std::array<GLint, MELEE_HOST_GX_MAX_TEXMAP> formats{};
            for (std::size_t map = 0; map < set.size(); ++map) {
                if (set[map] < texture_formats.size()) {
                    formats[map] = static_cast<GLint>(texture_formats[set[map]]);
                }
            }
            set_program_uniforms(*program, mvp, tev, state, formats);
            for (std::size_t map = 0; map < set.size(); ++map) {
                glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + map));
                glBindTexture(GL_TEXTURE_2D, set[map] < textures.size()
                                                 ? textures[set[map]]
                                                 : white_texture);
            }
            glDrawArrays(GL_TRIANGLES, run.first, run.count);
        }

        /* Puts back what draw_run changes, for whatever draws next. */
        void restore_draw_defaults()
        {
            glActiveTexture(GL_TEXTURE0);
            glDepthMask(GL_TRUE);
            glDisable(GL_CULL_FACE);
            glDisable(GL_BLEND);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }

        /* Renders every run of the capture with the current framebuffer bound.
         */
        void draw_capture(const Capture& capture, ProgramCache* programs,
                          const std::vector<GLuint>& textures,
                          const std::vector<std::uint32_t>& texture_formats,
                          GLuint white_texture, const Mat4& mvp,
                          bool front_face_cw)
        {
            for (const DrawRun& run : capture.runs) {
                draw_run(run, programs, textures, texture_formats,
                         white_texture, mvp,
                         front_face_cw);
            }
            restore_draw_defaults();
        }

        struct GlWindow {
            SDL_Window* window = nullptr;
            SDL_GLContext context = nullptr;
        };

        bool open_gl_window(bool hidden, GlWindow* out, std::string* error)
        {
            if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
                *error = SDL_GetError();
                return false;
            }
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
            SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                                SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
            SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
            if (hidden) {
                flags |= SDL_WINDOW_HIDDEN;
            }
            out->window =
                SDL_CreateWindow("Melee PC — preview: setas/analógico direito "
                                 "movem a câmera; Back/Esc sai",
                                 960, 720, flags);
            if (out->window == nullptr) {
                *error = SDL_GetError();
                SDL_Quit();
                return false;
            }
            out->context = SDL_GL_CreateContext(out->window);
            if (out->context == nullptr) {
                *error = SDL_GetError();
                SDL_DestroyWindow(out->window);
                SDL_Quit();
                return false;
            }
#if defined(_WIN32)
            if (!load_modern_gl_functions(error)) {
                SDL_GL_DestroyContext(out->context);
                SDL_DestroyWindow(out->window);
                SDL_Quit();
                return false;
            }
#endif
            return true;
        }

        void close_gl_window(GlWindow* window)
        {
            SDL_GL_DestroyContext(window->context);
            SDL_DestroyWindow(window->window);
            SDL_Quit();
        }

        /* A colour and depth target of the given size, for rendering without a
         * visible window. */
        struct OffscreenTarget {
            GLuint framebuffer = 0;
            GLuint color = 0;
            GLuint depth = 0;
            GLuint resolve_framebuffer = 0;
            GLuint resolve_color = 0;

            OffscreenTarget(GLsizei target_width, GLsizei target_height,
                            int msaa = 0)
            {
                width = target_width;
                height = target_height;
                glGenFramebuffers(1, &framebuffer);
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
                glGenRenderbuffers(1, &color);
                glBindRenderbuffer(GL_RENDERBUFFER, color);
                
                if (msaa > 1) {
                    glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, GL_RGBA8, target_width, target_height);
                } else {
                    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, target_width, target_height);
                }
                
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_RENDERBUFFER, color);
                glGenRenderbuffers(1, &depth);
                glBindRenderbuffer(GL_RENDERBUFFER, depth);
                
                if (msaa > 1) {
                    glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, GL_DEPTH_COMPONENT24, target_width, target_height);
                } else {
                    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, target_width, target_height);
                }
                
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                          GL_RENDERBUFFER, depth);
                                          
                if (msaa > 1) {
                    glGenFramebuffers(1, &resolve_framebuffer);
                    glBindFramebuffer(GL_FRAMEBUFFER, resolve_framebuffer);
                    glGenRenderbuffers(1, &resolve_color);
                    glBindRenderbuffer(GL_RENDERBUFFER, resolve_color);
                    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, target_width, target_height);
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, resolve_color);
                }
            }

            OffscreenTarget(const OffscreenTarget&) = delete;
            OffscreenTarget& operator=(const OffscreenTarget&) = delete;

            ~OffscreenTarget()
            {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glDeleteRenderbuffers(1, &depth);
                glDeleteRenderbuffers(1, &color);
                glDeleteFramebuffers(1, &framebuffer);
                if (resolve_framebuffer != 0) {
                    glDeleteRenderbuffers(1, &resolve_color);
                    glDeleteFramebuffers(1, &resolve_framebuffer);
                }
            }

            [[nodiscard]] bool complete() const
            {
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
                const bool primary = glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
                                     GL_FRAMEBUFFER_COMPLETE;
                if (resolve_framebuffer == 0) return primary;
                glBindFramebuffer(GL_FRAMEBUFFER, resolve_framebuffer);
                const bool resolve = glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
                                     GL_FRAMEBUFFER_COMPLETE;
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
                return primary && resolve;
            }

            [[nodiscard]] bool resolve_color_buffer() const
            {
                if (resolve_framebuffer == 0) {
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
                    return true;
                }
                glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolve_framebuffer);
                glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                                  GL_COLOR_BUFFER_BIT, GL_NEAREST);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, resolve_framebuffer);
                return glGetError() == GL_NO_ERROR;
            }

            GLsizei width = 0;
            GLsizei height = 0;
        };

        bool save_framebuffer_bmp(const char* path, int width, int height,
                                  std::string* error)
        {
            const auto row_bytes = static_cast<std::size_t>(width) * 4U;
            std::vector<std::uint8_t> pixels(row_bytes *
                                             static_cast<std::size_t>(height));
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                         pixels.data());
            /* A displayed frame has no alpha: the console scans out a YUV XFB.
             * The alpha a frame keeps for blending would make the file
             * transparent. */
            for (std::size_t alpha = 3; alpha < pixels.size(); alpha += 4) {
                pixels[alpha] = 255;
            }
            /* GL reads bottom-up; an image file is top-down. */
            std::vector<std::uint8_t> flipped(pixels.size());
            for (int row = 0; row < height; ++row) {
                const auto source =
                    static_cast<std::size_t>(height - 1 - row) * row_bytes;
                std::copy_n(
                    pixels.begin() + static_cast<std::ptrdiff_t>(source),
                    row_bytes,
                    flipped.begin() +
                        static_cast<std::ptrdiff_t>(
                            static_cast<std::size_t>(row) * row_bytes));
            }
            SDL_Surface* const surface = SDL_CreateSurfaceFrom(
                width, height, SDL_PIXELFORMAT_RGBA32, flipped.data(),
                static_cast<int>(row_bytes));
            if (surface == nullptr) {
                *error = SDL_GetError();
                return false;
            }
            const bool saved = SDL_SaveBMP(surface, path);
            if (!saved) {
                *error = SDL_GetError();
            }
            SDL_DestroySurface(surface);
            return saved;
        }

        MeleeHostPadState read_pad(SDL_Gamepad* gamepad, float* yaw,
                                   float* pitch, float* zoom, bool* running)
        {
            const bool* const keys = SDL_GetKeyboardState(nullptr);
            const auto axis = [](bool negative, bool positive) {
                return static_cast<mh_s8>((positive ? 127 : 0) -
                                          (negative ? 127 : 0));
            };
            MeleeHostPadState pad{
                .buttons = static_cast<mh_u16>(
                    (keys[SDL_SCANCODE_J] ? PAD_BUTTON_A : 0) |
                    (keys[SDL_SCANCODE_K] ? PAD_BUTTON_B : 0) |
                    (keys[SDL_SCANCODE_U] ? PAD_BUTTON_X : 0) |
                    (keys[SDL_SCANCODE_I] ? PAD_BUTTON_Y : 0) |
                    (keys[SDL_SCANCODE_Q] ? PAD_TRIGGER_Z : 0) |
                    (keys[SDL_SCANCODE_H] ? PAD_TRIGGER_L : 0) |
                    (keys[SDL_SCANCODE_L] ? PAD_TRIGGER_R : 0) |
                    (keys[SDL_SCANCODE_KP_8] ? PAD_BUTTON_UP : 0) |
                    (keys[SDL_SCANCODE_KP_2] ? PAD_BUTTON_DOWN : 0) |
                    (keys[SDL_SCANCODE_KP_4] ? PAD_BUTTON_LEFT : 0) |
                    (keys[SDL_SCANCODE_KP_6] ? PAD_BUTTON_RIGHT : 0) |
                    (keys[SDL_SCANCODE_RETURN] ? PAD_BUTTON_START : 0)),
                .stick_x = axis(keys[SDL_SCANCODE_A], keys[SDL_SCANCODE_D]),
                .stick_y = axis(keys[SDL_SCANCODE_S], keys[SDL_SCANCODE_W]),
                .c_stick_x =
                    axis(keys[SDL_SCANCODE_LEFT], keys[SDL_SCANCODE_RIGHT]),
                .c_stick_y =
                    axis(keys[SDL_SCANCODE_DOWN], keys[SDL_SCANCODE_UP]),
                .trigger_left =
                    static_cast<mh_u8>(keys[SDL_SCANCODE_H] ? 255U : 0U),
                .trigger_right =
                    static_cast<mh_u8>(keys[SDL_SCANCODE_L] ? 255U : 0U),
                .connected = true,
            };
            if (gamepad == nullptr) {
                return pad;
            }
            const auto gamepad_trigger = [](Sint16 value) {
                return static_cast<mh_u8>(
                    std::clamp(static_cast<int>(value) / 128, 0, 255));
            };
            const auto button = [gamepad](SDL_GamepadButton which, int bit) {
                return SDL_GetGamepadButton(gamepad, which) ? bit : 0;
            };
            pad.buttons = static_cast<mh_u16>(
                button(SDL_GAMEPAD_BUTTON_SOUTH, PAD_BUTTON_A) |
                button(SDL_GAMEPAD_BUTTON_EAST, PAD_BUTTON_B) |
                button(SDL_GAMEPAD_BUTTON_WEST, PAD_BUTTON_X) |
                button(SDL_GAMEPAD_BUTTON_NORTH, PAD_BUTTON_Y) |
                button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, PAD_TRIGGER_Z) |
                button(SDL_GAMEPAD_BUTTON_DPAD_UP, PAD_BUTTON_UP) |
                button(SDL_GAMEPAD_BUTTON_DPAD_DOWN, PAD_BUTTON_DOWN) |
                button(SDL_GAMEPAD_BUTTON_DPAD_LEFT, PAD_BUTTON_LEFT) |
                button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, PAD_BUTTON_RIGHT) |
                button(SDL_GAMEPAD_BUTTON_START, PAD_BUTTON_START));
            pad.stick_x = gamepad_axis_x(
                SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX));
            pad.stick_y = gamepad_axis_y(
                SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY));
            pad.c_stick_x = gamepad_axis_x(
                SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX));
            pad.c_stick_y = gamepad_axis_y(
                SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY));
            pad.trigger_left = gamepad_trigger(
                SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
            pad.trigger_right = gamepad_trigger(
                SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
            /* A GameCube trigger clicks at the end of its travel, which is
             * what the game's digital L and R read (the pause menu's
             * L+R+A+START). */
            if (pad.trigger_left >= 240U) {
                pad.buttons = static_cast<mh_u16>(pad.buttons | PAD_TRIGGER_L);
            }
            if (pad.trigger_right >= 240U) {
                pad.buttons = static_cast<mh_u16>(pad.buttons | PAD_TRIGGER_R);
            }

            /* The preview has no gameplay loop yet, so the C-stick and
             * triggers also move the camera while the complete PAD state still
             * reaches the host. */
            *yaw += static_cast<float>(pad.c_stick_x) / 127.0F * 2.5F;
            *pitch = std::clamp(*pitch - static_cast<float>(pad.c_stick_y) /
                                             127.0F * 2.5F,
                                -89.0F, 89.0F);
            const float zoom_axis = static_cast<float>(pad.trigger_right) -
                                    static_cast<float>(pad.trigger_left);
            *zoom =
                std::clamp(*zoom * (1.0F - zoom_axis / 8192.0F), 0.1F, 100.0F);
            if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_BACK)) {
                *running = false;
            }
            return pad;
        }

    } // namespace

    bool show_captured_geometry(MeleeHostContext* context, std::string* error,
                                FrameCallback on_frame, void* user_data)
    {
        std::string ignored;
        std::string& message = error != nullptr ? *error : ignored;
        if (melee_host_gx_triangle_count() == 0) {
            message = "there is no captured geometry to draw";
            return false;
        }
        if (context == nullptr) {
            message = "no host context";
            return false;
        }
        const char* const screenshot_path =
            std::getenv("MELEE_HOST_SCREENSHOT");
        const bool screenshot =
            screenshot_path != nullptr && screenshot_path[0] != '\0';

        GlWindow window;
        if (!open_gl_window(screenshot, &window, &message)) {
            return false;
        }
        SDL_GL_SetSwapInterval(1);
        int gamepad_count = 0;
        SDL_JoystickID* gamepad_ids = SDL_GetGamepads(&gamepad_count);
        SDL_Gamepad* gamepad =
            gamepad_count > 0 ? SDL_OpenGamepad(gamepad_ids[0]) : nullptr;
        SDL_free(gamepad_ids);

        constexpr std::array<std::uint8_t, 4> kWhite{ 255, 255, 255, 255 };
        const GLuint white_texture = create_texture(
            { 1, 1, 1, 1, { kWhite.begin(), kWhite.end() }, false, false });
        std::vector<GLuint> textures;
        std::vector<std::uint32_t> texture_formats;
        textures.reserve(texture_images.size());
        texture_formats.reserve(texture_images.size());
        for (const auto& image : texture_images) {
            textures.push_back(create_texture(image));
            texture_formats.push_back(image.gx_format);
        }

        bool shown = true;
        {
            ProgramCache programs;
            GLuint vertex_array = 0;
            GLuint vertex_buffer = 0;
            glGenVertexArrays(1, &vertex_array);
            glBindVertexArray(vertex_array);
            glGenBuffers(1, &vertex_buffer);
            glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
            bind_vertex_layout();

            Capture capture;
            const auto upload = [&capture]() {
                capture = read_capture();
                glBufferData(GL_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(capture.vertices.size() *
                                                     sizeof(float)),
                             capture.vertices.data(), GL_DYNAMIC_DRAW);
            };
            upload();
            /* The camera frames the first capture and stays put, so an
             * animation moves inside the frame instead of the frame chasing
             * it. */
            const Bounds framing = capture.bounds;

            bool running = true;
            /* GX treats a clockwise winding as the front face.  The viewer
             * starts there and can flip it, because a model that looks inside
             * out is the clearest evidence the assumption is wrong for a given
             * asset. */
            bool front_face_cw = true;
            float yaw = 20.0F;
            float pitch = -20.0F;
            float zoom = 1.0F;
            const auto render_into = [&](int width, int height) {
                glViewport(0, 0, width, height);
                glClearColor(0.035F, 0.045F, 0.08F, 1.0F);
                glDepthMask(GL_TRUE);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                draw_capture(
                    capture, &programs, textures, texture_formats,
                    white_texture,
                    preview_mvp(framing, width, height, yaw, pitch, zoom),
                    front_face_cw);
            };

            if (screenshot) {
                constexpr int kWidth = 960;
                constexpr int kHeight = 720;
                OffscreenTarget target(kWidth, kHeight);
                if (!target.complete()) {
                    message = "offscreen framebuffer is incomplete";
                    shown = false;
                } else {
                    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
                    render_into(kWidth, kHeight);
                    if (!target.resolve_color_buffer()) {
                        message = "could not resolve offscreen framebuffer";
                        shown = false;
                    } else {
                    shown = save_framebuffer_bmp(screenshot_path, kWidth,
                                                 kHeight, &message);
                    }
                }
                running = false;
            }

            while (running) {
                SDL_Event event{};
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_EVENT_QUIT) {
                        running = false;
                    } else if (event.type == SDL_EVENT_GAMEPAD_ADDED &&
                               gamepad == nullptr)
                    {
                        gamepad = SDL_OpenGamepad(event.gdevice.which);
                    } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED &&
                               gamepad != nullptr &&
                               SDL_GetGamepadID(gamepad) ==
                                   event.gdevice.which)
                    {
                        SDL_CloseGamepad(gamepad);
                        gamepad = nullptr;
                    } else if (event.type == SDL_EVENT_KEY_DOWN) {
                        switch (event.key.scancode) {
                        case SDL_SCANCODE_ESCAPE:
                            running = false;
                            break;
                        case SDL_SCANCODE_LEFT:
                            yaw -= 5.0F;
                            break;
                        case SDL_SCANCODE_RIGHT:
                            yaw += 5.0F;
                            break;
                        case SDL_SCANCODE_UP:
                            pitch = std::min(pitch + 5.0F, 89.0F);
                            break;
                        case SDL_SCANCODE_DOWN:
                            pitch = std::max(pitch - 5.0F, -89.0F);
                            break;
                        case SDL_SCANCODE_PAGEUP:
                            zoom = std::max(zoom * 0.9F, 0.1F);
                            break;
                        case SDL_SCANCODE_PAGEDOWN:
                            zoom = std::min(zoom * 1.1F, 100.0F);
                            break;
                        case SDL_SCANCODE_F:
                            front_face_cw = !front_face_cw;
                            break;
                        default:
                            break;
                        }
                    }
                }
                const MeleeHostPadState pad =
                    read_pad(gamepad, &yaw, &pitch, &zoom, &running);
                if (melee_host_submit_pad_state(context, 0, &pad) !=
                    MELEE_HOST_OK)
                {
                    message = "could not submit SDL input";
                    running = false;
                }
                static_cast<void>(melee_host_step(context));
                if (on_frame != nullptr) {
                    /* The viewer advances the animation and captures again
                     * here, so the geometry drawn below is this frame's. */
                    on_frame(user_data);
                    upload();
                }
                int width = 0;
                int height = 0;
                SDL_GetWindowSizeInPixels(window.window, &width, &height);
                if (width <= 0 || height <= 0) {
                    continue;
                }
                render_into(width, height);
                SDL_GL_SwapWindow(window.window);
            }

            glDeleteBuffers(1, &vertex_buffer);
            glDeleteVertexArrays(1, &vertex_array);
        }

        glDeleteTextures(1, &white_texture);
        if (!textures.empty()) {
            glDeleteTextures(static_cast<GLsizei>(textures.size()),
                             textures.data());
        }
        if (gamepad != nullptr) {
            SDL_CloseGamepad(gamepad);
        }
        close_gl_window(&window);
        return shown;
    }

    bool run_tev_conformance(std::size_t cases_per_program,
                             TevConformanceReport* report, std::string* error)
    {
        std::string ignored;
        std::string& message = error != nullptr ? *error : ignored;
        TevConformanceReport result;

        /* The pairs the capture actually drew, since the alpha test belongs to
         * the pixel state and the colour to the program. */
        std::set<std::pair<mh_u32, mh_u32>> pairs;
        const std::size_t triangle_count = melee_host_gx_triangle_count();
        for (std::size_t index = 0; index < triangle_count; ++index) {
            MeleeHostGxCapturedTriangle triangle{};
            if (melee_host_gx_captured_triangle_at(index, &triangle)) {
                pairs.emplace(triangle.vertices[0].tev_state,
                              triangle.vertices[0].draw_state);
            }
        }
        if (pairs.empty()) {
            message = "there is no captured geometry to check";
            return false;
        }

        GlWindow window;
        if (!open_gl_window(true, &window, &message)) {
            return false;
        }
        bool ok = true;
        {
            ProgramCache programs;
            OffscreenTarget target(1, 1);
            if (!target.complete()) {
                message = "offscreen framebuffer is incomplete";
                ok = false;
            }
            GLuint vertex_array = 0;
            GLuint vertex_buffer = 0;
            glGenVertexArrays(1, &vertex_array);
            glBindVertexArray(vertex_array);
            glGenBuffers(1, &vertex_buffer);
            glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
            bind_vertex_layout();
            std::array<GLuint, MELEE_HOST_GX_MAX_TEXMAP> textures{};
            glGenTextures(static_cast<GLsizei>(textures.size()),
                          textures.data());
            for (GLuint texture : textures) {
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                GL_NEAREST);
            }
            glViewport(0, 0, 1, 1);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glDisable(GL_BLEND);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);

            std::mt19937 random(0x4D454C45U);
            std::uniform_int_distribution<int> byte(0, 255);
            const auto random_rgba = [&]() {
                return std::array<int, 4>{ byte(random), byte(random),
                                           byte(random), byte(random) };
            };

            std::set<mh_u32> programs_seen;
            for (const auto& [tev_id, draw_id] : pairs) {
                if (!ok) {
                    break;
                }
                MeleeHostGxTevState tev{};
                MeleeHostGxDrawState state{};
                if (!melee_host_gx_captured_tev_state_at(tev_id, &tev) ||
                    !melee_host_gx_captured_draw_state_at(draw_id, &state))
                {
                    continue;
                }
                const TevProgram* const program = programs.get(tev,
                                                                 state.indirect);
                if (program == nullptr) {
                    message = "TEV program " + std::to_string(tev_id) +
                              " did not compile";
                    ok = false;
                    break;
                }
                programs_seen.insert(tev_id);
                /* Half the cases run with the fog the capture holds and half
                 * with a linear fog the quad's own depth falls in the middle
                 * of, which is the only way this check reaches the curve: a
                 * scene's fog starts far beyond the quad. */
                MeleeHostGxDrawState probe = state;
                probe.fog_type = 2; /* GX_FOG_LIN */
                probe.fog_start_z = 0.5F;
                probe.fog_end_z = 1.5F;
                probe.fog_color[0] = 40;
                probe.fog_color[1] = 80;
                probe.fog_color[2] = 160;
                probe.fog_color[3] = 255;
                set_program_uniforms(*program, identity(), tev, state, {});

                for (std::size_t sample = 0; sample < cases_per_program;
                     ++sample)
                {
                    const bool probing = sample >= cases_per_program / 2;
                    const MeleeHostGxDrawState& fog_state =
                        probing ? probe : state;
                    if (probing && sample == cases_per_program / 2) {
                        set_program_uniforms(*program, identity(), tev, probe,
                                             {});
                    }
                    melee::gx::TevFragmentInputs inputs{};
                    inputs.raster[0] = random_rgba();
                    inputs.raster[1] = random_rgba();
                    for (std::size_t map = 0; map < textures.size(); ++map) {
                        inputs.texmap[map] = random_rgba();
                        std::array<std::uint8_t, 4> texel{};
                        for (std::size_t c = 0; c < 4; ++c) {
                            texel[c] = static_cast<std::uint8_t>(
                                inputs.texmap[map][c]);
                        }
                        glActiveTexture(
                            static_cast<GLenum>(GL_TEXTURE0 + map));
                        glBindTexture(GL_TEXTURE_2D, textures[map]);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
                                     GL_RGBA, GL_UNSIGNED_BYTE, texel.data());
                    }

                    /* One triangle covering the pixel, every attribute
                     * constant so interpolation hands the inputs through
                     * unchanged. */
                    MeleeHostGxCapturedVertex vertex{};
                    for (std::size_t channel = 0; channel < 2; ++channel) {
                        for (std::size_t c = 0; c < 4; ++c) {
                            vertex.raster_color[channel][c] =
                                static_cast<mh_u8>(inputs.raster[channel][c]);
                        }
                    }
                    for (auto& coord : vertex.texgen) {
                        coord[0] = 0.5F;
                        coord[1] = 0.5F;
                        coord[2] = 1.0F;
                    }
                    std::vector<float> data;
                    constexpr std::array<std::array<float, 2>, 3> kCorners{
                        { { -1.0F, -1.0F }, { 3.0F, -1.0F }, { -1.0F, 3.0F } }
                    };
                    for (const auto& corner : kCorners) {
                        vertex.position = { corner[0], corner[1], 0.0F };
                        append_vertex(&data, vertex);
                    }
                    glBufferData(
                        GL_ARRAY_BUFFER,
                        static_cast<GLsizeiptr>(data.size() * sizeof(float)),
                        data.data(), GL_STREAM_DRAW);

                    std::array<int, 4> expected =
                        melee::gx::evaluate_tev(tev, inputs);
                    /* The quad is drawn with an identity transform, so the
                     * fragment's clip w is one and the shader's fog reads that
                     * depth; the expected colour goes through the same curve.
                     */
                    const int fog = melee::gx::fog_weight(fog_state, 1.0F);
                    if (fog != 0) {
                        for (std::size_t c = 0; c < 3; ++c) {
                            expected[c] = melee::gx::fog_mix(
                                std::clamp(expected[c], 0, 255),
                                fog_state.fog_color[c], fog);
                        }
                    }
                    const bool passes =
                        melee::gx::alpha_test_passes(state, expected[3]);

                    /* Drawn over two different clears: a discarded fragment
                     * shows each clear, a written one the same colour twice.
                     */
                    std::array<std::array<std::uint8_t, 4>, 2> pixels{};
                    constexpr std::array<std::array<float, 4>, 2> kClears{
                        { { 1.0F, 0.0F, 1.0F, 0.0F },
                          { 0.0F, 1.0F, 0.0F, 1.0F } }
                    };
                    for (std::size_t pass = 0; pass < 2; ++pass) {
                        glClearColor(kClears[pass][0], kClears[pass][1],
                                     kClears[pass][2], kClears[pass][3]);
                        glClear(GL_COLOR_BUFFER_BIT);
                        glDrawArrays(GL_TRIANGLES, 0, 3);
                        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                                     pixels[pass].data());
                    }
                    const auto matches =
                        [](const std::array<std::uint8_t, 4>& p,
                           const std::array<int, 4>& value) {
                            return p[0] == value[0] && p[1] == value[1] &&
                                   p[2] == value[2] && p[3] == value[3];
                        };
                    const bool discarded =
                        matches(pixels[0], { 255, 0, 255, 0 }) &&
                        matches(pixels[1], { 0, 255, 0, 255 });
                    const bool agrees = passes
                                            ? matches(pixels[0], expected) &&
                                                  matches(pixels[1], expected)
                                            : discarded;
                    result.cases += 1;
                    if (!agrees) {
                        result.mismatches += 1;
                        if (result.first_mismatch.empty()) {
                            const auto text = [](const auto& p) {
                                return std::to_string(p[0]) + "," +
                                       std::to_string(p[1]) + "," +
                                       std::to_string(p[2]) + "," +
                                       std::to_string(p[3]);
                            };
                            result.first_mismatch =
                                "tev " + std::to_string(tev_id) + " draw " +
                                std::to_string(draw_id) + ": expected " +
                                (passes ? text(expected)
                                        : std::string("discard")) +
                                ", shader wrote " + text(pixels[0]) + " / " +
                                text(pixels[1]);
                        }
                    }
                }
            }
            result.programs = programs_seen.size();

            glDeleteTextures(static_cast<GLsizei>(textures.size()),
                             textures.data());
            glDeleteBuffers(1, &vertex_buffer);
            glDeleteVertexArrays(1, &vertex_array);
        }
        close_gl_window(&window);
        if (report != nullptr) {
            *report = result;
        }
        return ok;
    }

    void set_texture_images(std::vector<TextureImage> images)
    {
        texture_images = std::move(images);
    }

    struct FramePresenter::State {
        GlWindow window;
        std::unique_ptr<OffscreenTarget> target;
        bool hidden = false;
        std::unique_ptr<ProgramCache> programs;
        GLuint vertex_array = 0;
        GLuint vertex_buffer = 0;
        GLuint white_texture = 0;
        /* The GL texture of each image, and the generation it was uploaded at.
         */
        std::map<const TextureImage*, std::pair<GLuint, std::uint32_t>>
            textures;
        SDL_Gamepad* gamepad = nullptr;
        SDL_AudioStream* audio = nullptr;
        int width = 0;
        int height = 0;
        int render_scale = 1;
        Uint64 next_sim_ns = 0;
        Uint64 next_blit_ns = 0;
        Uint64 next_pace_ns = 0;
        VideoSettings video_settings{};
        bool video_menu_open = false;
        std::uint8_t video_menu_row = 0;
        GLuint video_menu_list = 0;
        bool video_menu_dirty = true;
        std::string settings_path;
        bool settings_ui_initialized = false;
        /* Twice a second a visible window shows the rate it presents at. */
        FrameRateMeter frame_rate{ 500'000'000ULL };
        double last_fps = 0.0;
    };

    bool configure_render_target(FramePresenter::State& state,
                                 std::string* error)
    {
        MeleeHostVideoState video{};
        static_cast<void>(melee_host_video_state(&video));
        const int scale = resolution_scale(state.video_settings.resolution);
        int requested_samples = 0;
        switch (state.video_settings.anti_aliasing) {
        case PresentationAntiAliasing::Msaa2x: requested_samples = 2; break;
        case PresentationAntiAliasing::Msaa4x: requested_samples = 4; break;
        case PresentationAntiAliasing::Msaa8x: requested_samples = 8; break;
        case PresentationAntiAliasing::Off: break;
        }
        if (requested_samples > 1) {
            GLint maximum_samples = 0;
            glGetIntegerv(GL_MAX_SAMPLES, &maximum_samples);
            if (maximum_samples < requested_samples) {
                if (error != nullptr) {
                    *error = "requested MSAA level is unsupported by this GPU";
                }
                return false;
            }
        }
        auto target = std::make_unique<OffscreenTarget>(
            static_cast<GLsizei>(video.framebuffer_width * scale),
            static_cast<GLsizei>(video.embedded_framebuffer_height * scale),
            requested_samples);
        if (!target->complete()) {
            if (error != nullptr) *error = "video render target is incomplete";
            return false;
        }
        state.target = std::move(target);
        state.render_scale = scale;
        return true;
    }

    std::string video_settings_path()
    {
        char* directory = SDL_GetPrefPath("MeleePC", "MeleePC");
        if (directory == nullptr) {
            return {};
        }
        std::string path(directory);
        SDL_free(directory);
        return path + "video-settings.txt";
    }

    void load_video_settings(FramePresenter::State* state)
    {
        if (state->settings_path.empty()) {
            return;
        }
        std::ifstream input(state->settings_path);
        int resolution = -1;
        int aspect = -1;
        int filter = -1;
        int window_mode = -1;
        int rate = 0;
        int show_fps = 0;
        int custom_textures = 1;
        int anti_aliasing = 0;
        int anisotropy = 0;
        if (!(input >> resolution >> aspect >> filter >> window_mode >> rate >> show_fps) ||
            resolution < 0 || resolution > 4 ||
            aspect < 0 || aspect > 2 ||
            filter < 0 || filter > 1 ||
            window_mode < 0 || window_mode > 2 ||
            (rate != 60 && rate != 120 && rate != 144 && rate != 165 && rate != 240 && rate != 0))
        {
            return;
        }
        
        // Optional newer fields
        if (!(input >> custom_textures)) custom_textures = 1;
        if (!(input >> anti_aliasing)) anti_aliasing = 0;
        if (!(input >> anisotropy)) anisotropy = 0;
        
        if (anti_aliasing < 0 || anti_aliasing > 3) anti_aliasing = 0;
        if (anisotropy < 0 || anisotropy > 4) anisotropy = 0;
        state->video_settings.show_fps = show_fps != 0;
        state->video_settings.custom_textures = custom_textures != 0;
        set_custom_textures_enabled(state->video_settings.custom_textures);
        state->video_settings.anti_aliasing = static_cast<PresentationAntiAliasing>(anti_aliasing);
        state->video_settings.anisotropy = static_cast<PresentationAnisotropy>(anisotropy);
        if (anisotropy == 1) g_anisotropy_level = 2;
        else if (anisotropy == 2) g_anisotropy_level = 4;
        else if (anisotropy == 3) g_anisotropy_level = 8;
        else if (anisotropy == 4) g_anisotropy_level = 16;
        else g_anisotropy_level = 0;
        state->video_settings.resolution =
            static_cast<PresentationResolution>(resolution);
        state->video_settings.aspect =
            static_cast<PresentationAspect>(aspect);
        state->video_settings.filter =
            static_cast<PresentationFilter>(filter);
        state->video_settings.window_mode =
            static_cast<PresentationWindowMode>(window_mode);
        /* Version 1 stored zero for the removed unlimited option.  Preserve
         * the rest of that settings file but migrate it to the safe default. */
        state->video_settings.rate = static_cast<PresentationRate>(
            rate == 0 ? 60 : rate);
    }

    void save_video_settings(const FramePresenter::State& state)
    {
        if (state.settings_path.empty()) {
            return;
        }
        std::ofstream output(state.settings_path, std::ios::trunc);
        if (output) {
            output << static_cast<int>(state.video_settings.resolution) << ' '
                   << static_cast<int>(state.video_settings.aspect) << ' '
                   << static_cast<int>(state.video_settings.filter) << ' '
                   << static_cast<int>(state.video_settings.window_mode) << ' '
                   << static_cast<int>(state.video_settings.rate) << ' '
                   << (state.video_settings.show_fps ? 1 : 0) << ' '
                   << (state.video_settings.custom_textures ? 1 : 0) << ' '
                   << static_cast<int>(state.video_settings.anti_aliasing) << ' '
                   << static_cast<int>(state.video_settings.anisotropy) << '\n';
        }
    }

    std::string video_menu_title(const VideoSettings& settings, bool open,
                                 std::uint8_t row,
                                 double frames_per_second = 0.0)
    {
        std::string title = frames_per_second > 0.0
                                ? play_window_title(frames_per_second)
                                : play_window_title();
        if (!open) {
            return title;
        }
        const char* value = video_menu_row_value(settings, row);
        return title + " — VIDEO: " + kVideoMenuRowNames[row] + " = " +
               value + " (arrows, Enter, Esc)";
    }

    std::array<std::uint8_t, 7> menu_glyph(char character)
    {
        /* Map lowercase to uppercase so callers can pass mixed case. */
        if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(character - 'a' + 'A');
        }
        switch (character) {
        case 'A':
            return { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 };
        case 'B':
            return { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E };
        case 'C':
            return { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E };
        case 'D':
            return { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E };
        case 'E':
            return { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F };
        case 'F':
            return { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 };
        case 'G':
            return { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E };
        case 'H':
            return { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 };
        case 'I':
            return { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F };
        case 'J':
            return { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C };
        case 'K':
            return { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 };
        case 'L':
            return { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F };
        case 'M':
            return { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 };
        case 'N':
            return { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 };
        case 'O':
            return { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E };
        case 'P':
            return { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 };
        case 'Q':
            return { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D };
        case 'R':
            return { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 };
        case 'S':
            return { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E };
        case 'T':
            return { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 };
        case 'U':
            return { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E };
        case 'V':
            return { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 };
        case 'W':
            return { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11 };
        case 'X':
            return { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 };
        case 'Y':
            return { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 };
        case 'Z':
            return { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F };
        case '0':
            return { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E };
        case '1':
            return { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E };
        case '2':
            return { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F };
        case '3':
            return { 0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E };
        case '4':
            return { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 };
        case '5':
            return { 0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E };
        case '6':
            return { 0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E };
        case '7':
            return { 0x1F, 0x01, 0x02, 0x04, 0x04, 0x04, 0x04 };
        case '8':
            return { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E };
        case '9':
            return { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E };
        case ':':
            return { 0, 0x04, 0x04, 0, 0x04, 0x04, 0 };
        case '<':
            return { 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 };
        case '>':
            return { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 };
        case '-':
            return { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 };
        case ' ':
            return {};
        default:
            return {};
        }
    }

    void menu_rect(float left, float top, float right, float bottom)
    {
        glBegin(GL_QUADS);
        glVertex2f(left, top);
        glVertex2f(right, top);
        glVertex2f(right, bottom);
        glVertex2f(left, bottom);
        glEnd();
    }

    /* Draws a rounded-corner rectangle by inset. */
    void menu_rect_rounded(float left, float top, float right, float bottom,
                           float radius)
    {
        /* Approximate rounded rect: draw the body and four edge strips
         * inset by `radius`, skipping the corner pixels. For a simple GL
         * immediate-mode overlay this is good enough. */
        menu_rect(left + radius, top, right - radius, bottom);
        menu_rect(left, top + radius, left + radius, bottom - radius);
        menu_rect(right - radius, top + radius, right, bottom - radius);
    }

    void menu_text(float x, float y, float scale, const char* text)
    {
        glBegin(GL_QUADS);
        for (; *text != '\0'; ++text, x += scale * 6.0F) {
            const auto glyph = menu_glyph(*text);
            for (std::size_t row = 0; row < glyph.size(); ++row) {
                for (int column = 0; column < 5; ++column) {
                    if ((glyph[row] & (1U << (4 - column))) != 0) {
                        const float column_f = static_cast<float>(column);
                        const float row_f = static_cast<float>(row);
                        glVertex2f(x + column_f * scale, y + row_f * scale);
                        glVertex2f(x + (column_f + 1.0F) * scale,
                                   y + row_f * scale);
                        glVertex2f(x + (column_f + 1.0F) * scale,
                                   y + (row_f + 1.0F) * scale);
                        glVertex2f(x + column_f * scale,
                                   y + (row_f + 1.0F) * scale);
                    }
                }
            }
        }
        glEnd();
    }

    void apply_window_mode(FramePresenter::State& state)
    {
        switch (state.video_settings.window_mode) {
        case PresentationWindowMode::Windowed:
            SDL_SetWindowFullscreen(state.window.window, false);
            SDL_SetWindowBordered(state.window.window, true);
            break;
        case PresentationWindowMode::Fullscreen:
            SDL_SetWindowFullscreen(state.window.window, true);
            break;
        case PresentationWindowMode::Borderless:
            SDL_SetWindowFullscreen(state.window.window, false);
            SDL_SetWindowBordered(state.window.window, false);
            SDL_MaximizeWindow(state.window.window);
            break;
        }
    }

    void draw_fps_counter(FramePresenter::State& state)
    {
        if (!state.video_settings.show_fps) {
            return;
        }
        int win_w = 0;
        int win_h = 0;
        SDL_GetWindowSizeInPixels(state.window.window, &win_w, &win_h);
        if (win_w <= 0 || win_h <= 0) {
            win_w = 960;
            win_h = 720;
        }

        glUseProgram(0);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, win_w, win_h, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        char fps_text[32];
        std::snprintf(fps_text, sizeof(fps_text), "%.0f FPS", state.last_fps);

        glColor4f(0.0F, 0.0F, 0.0F, 1.0F);
        menu_text(12.0F, 12.0F, 3.0F, fps_text);
        
        glColor4f(0.1F, 1.0F, 0.3F, 1.0F);
        menu_text(10.0F, 10.0F, 3.0F, fps_text);

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        restore_draw_defaults();
    }

    void draw_video_menu(FramePresenter::State& state)
    {
        if (!state.video_menu_open) {
            return;
        }
        if (state.video_menu_list == 0) {
            state.video_menu_list = glGenLists(1);
            state.video_menu_dirty = true;
        }
        if (state.video_menu_dirty) {
            glNewList(state.video_menu_list, GL_COMPILE);

        /* Read actual window dimensions for layout. */
        int win_w = 0;
        int win_h = 0;
        SDL_GetWindowSizeInPixels(state.window.window, &win_w, &win_h);
        if (win_w <= 0 || win_h <= 0) {
            win_w = 960;
            win_h = 720;
        }

        glUseProgram(0);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, win_w, win_h, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        /* Panel dimensions scale with window size for a polished look. */
        constexpr float kPanelWidth = 520.0F;
        constexpr float kRowHeight = 40.0F;
        constexpr float kRowGap = 6.0F;
        constexpr float kHeaderHeight = 50.0F;
        constexpr float kFooterHeight = 32.0F;
        constexpr float kPadding = 20.0F;
        const float panel_height =
            kHeaderHeight + kVideoMenuRows * (kRowHeight + kRowGap) +
            kFooterHeight + kPadding;
        const float panel_left =
            std::max(8.0F, (static_cast<float>(win_w) - kPanelWidth) * 0.5F);
        const float panel_top =
            std::max(8.0F, (static_cast<float>(win_h) - panel_height) * 0.5F);
        const float panel_right = panel_left + kPanelWidth;
        const float panel_bottom = panel_top + panel_height;

        /* Dark semi-transparent background. */
        glColor4f(0.02F, 0.03F, 0.08F, 0.92F);
        menu_rect(panel_left, panel_top, panel_right, panel_bottom);

        /* Subtle border highlight. */
        glColor4f(0.20F, 0.35F, 0.65F, 0.60F);
        menu_rect(panel_left, panel_top, panel_right, panel_top + 2.0F);
        menu_rect(panel_left, panel_bottom - 2.0F, panel_right, panel_bottom);
        menu_rect(panel_left, panel_top, panel_left + 2.0F, panel_bottom);
        menu_rect(panel_right - 2.0F, panel_top, panel_right, panel_bottom);

        /* Header: "VIDEO" title. */
        glColor4f(0.90F, 0.95F, 1.0F, 1.0F);
        menu_text(panel_left + kPadding, panel_top + 16.0F, 4.0F, "VIDEO");

        /* Resolution info under title. */
        {
            char res_info[64];
            MeleeHostVideoState video{};
            static_cast<void>(melee_host_video_state(&video));
            const int scale = resolution_scale(state.video_settings.resolution);
            std::snprintf(res_info, sizeof(res_info), "%dx%d",
                          video.framebuffer_width * scale,
                          video.embedded_framebuffer_height * scale);
            glColor4f(0.50F, 0.60F, 0.80F, 0.80F);
            menu_text(panel_left + kPadding + 160.0F, panel_top + 20.0F,
                      2.5F, res_info);
        }

        /* Each settings row. */
        for (std::uint8_t row = 0; row < kVideoMenuRows; ++row) {
            const float y =
                panel_top + kHeaderHeight + row * (kRowHeight + kRowGap);
            const bool selected = (row == state.video_menu_row);

            /* Row background. */
            if (selected) {
                glColor4f(0.10F, 0.25F, 0.55F, 0.95F);
            } else {
                glColor4f(0.06F, 0.08F, 0.16F, 0.80F);
            }
            menu_rect(panel_left + 10.0F, y,
                      panel_right - 10.0F, y + kRowHeight);

            /* Selected row accent bar on the left. */
            if (selected) {
                glColor4f(0.30F, 0.60F, 1.0F, 1.0F);
                menu_rect(panel_left + 10.0F, y,
                          panel_left + 14.0F, y + kRowHeight);
            }

            /* Row name. */
            glColor4f(selected ? 1.0F : 0.70F,
                      selected ? 1.0F : 0.75F,
                      selected ? 1.0F : 0.85F, 1.0F);
            menu_text(panel_left + 24.0F, y + 10.0F, 3.0F,
                      kVideoMenuRowNames[row]);

            /* Value with arrow indicators. */
            const char* value = video_menu_row_value(
                state.video_settings, row);
            if (selected) {
                /* Left arrow. */
                glColor4f(0.50F, 0.75F, 1.0F, 1.0F);
                menu_text(panel_right - 210.0F, y + 10.0F, 3.0F, "<");
            }
            glColor4f(selected ? 1.0F : 0.60F,
                      selected ? 1.0F : 0.65F,
                      selected ? 1.0F : 0.75F, 1.0F);
            menu_text(panel_right - 190.0F, y + 10.0F, 3.0F, value);
            if (selected) {
                /* Right arrow. */
                glColor4f(0.50F, 0.75F, 1.0F, 1.0F);
                menu_text(panel_right - 34.0F, y + 10.0F, 3.0F, ">");
            }
        }

        /* Footer hint. */
        glColor4f(0.40F, 0.50F, 0.65F, 0.70F);
        menu_text(panel_left + kPadding,
                  panel_bottom - kFooterHeight + 6.0F, 2.0F,
                  "ESC FECHA   SETAS NAVEGAM   ENTER CONFIRMA");

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        restore_draw_defaults();
            glEndList();
            state.video_menu_dirty = false;
        }
        glCallList(state.video_menu_list);
    }

    FramePresenter::FramePresenter() = default;

    FramePresenter::~FramePresenter()
    {
        if (state_ == nullptr) {
            return;
        }
        for (const auto& entry : state_->textures) {
            glDeleteTextures(1, &entry.second.first);
        }
        glDeleteTextures(1, &state_->white_texture);
        if (state_->video_menu_list != 0) {
            glDeleteLists(state_->video_menu_list, 1);
        }
        glDeleteBuffers(1, &state_->vertex_buffer);
        glDeleteVertexArrays(1, &state_->vertex_array);
        state_->programs.reset();
        state_->target.reset();
        if (state_->gamepad != nullptr) {
            SDL_CloseGamepad(state_->gamepad);
        }
        if (state_->audio != nullptr) {
            SDL_DestroyAudioStream(state_->audio);
        }
        if (state_->settings_ui_initialized) {
            destroy_settings_ui();
        }
        close_gl_window(&state_->window);
    }

    bool FramePresenter::open(bool hidden, std::string* error)
    {
        std::string ignored;
        std::string& message = error != nullptr ? *error : ignored;
        if (state_ != nullptr) {
            message = "the presenter is already open";
            return false;
        }
        auto state = std::make_unique<State>();
        if (!open_gl_window(hidden, &state->window, &message)) {
            return false;
        }
        state->settings_path = video_settings_path();
        load_video_settings(state.get());
        SDL_SetWindowTitle(state->window.window, play_window_title().c_str());
        SDL_GL_SetSwapInterval(1);
        int gamepad_count = 0;
        SDL_JoystickID* const gamepad_ids = SDL_GetGamepads(&gamepad_count);
        state->gamepad =
            gamepad_count > 0 ? SDL_OpenGamepad(gamepad_ids[0]) : nullptr;
        SDL_free(gamepad_ids);

        constexpr std::array<std::uint8_t, 4> kWhite{ 255, 255, 255, 255 };
        state->white_texture = create_texture(
            { 1, 1, 1, 1, { kWhite.begin(), kWhite.end() }, false, false });
        state->programs = std::make_unique<ProgramCache>();
        glGenVertexArrays(1, &state->vertex_array);
        glBindVertexArray(state->vertex_array);
        glGenBuffers(1, &state->vertex_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, state->vertex_buffer);
        bind_vertex_layout();

        state->hidden = hidden;
        if (!configure_render_target(*state, &message)) {
            /* Kept, so the destructor releases what was created. */
            state_ = std::move(state);
            return false;
        }
        state_ = std::move(state);
        /* Apply saved window mode (fullscreen, borderless, etc.). */
        if (!hidden) {
            apply_window_mode(*state_);
        }
        init_settings_ui(state_->window.window, state_->window.context);
        state_->settings_ui_initialized = true;
        return true;
    }

    void
    FramePresenter::present(const std::vector<const TextureImage*>& images)
    {
        if (state_ == nullptr) {
            return;
        }
        State& state = *state_;
        MeleeHostVideoState video{};
        static_cast<void>(melee_host_video_state(&video));
        const int framebuffer_width = video.framebuffer_width;
        const int framebuffer_height = video.embedded_framebuffer_height;
        glBindFramebuffer(GL_FRAMEBUFFER, state.target->framebuffer);
        state.width = framebuffer_width * state.render_scale;
        state.height = framebuffer_height * state.render_scale;
        if (state.width <= 0 || state.height <= 0) {
            return;
        }

        /* GX cleared the framebuffer to the display copy's colour and depth
         * when it copied the previous frame out. */
        MeleeHostGxDisplayCopyState copy{};
        melee_host_gx_display_copy_state(&copy);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, state.width, state.height);
        glDepthRange(0.0, 1.0);
        glDepthMask(GL_TRUE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor(static_cast<float>(copy.clear_color[0]) / 255.0F,
                     static_cast<float>(copy.clear_color[1]) / 255.0F,
                     static_cast<float>(copy.clear_color[2]) / 255.0F,
                     static_cast<float>(copy.clear_color[3]) / 255.0F);
        glClearDepth(static_cast<double>(copy.clear_depth) / 16777215.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        std::vector<GLuint> textures;
        std::vector<std::uint32_t> texture_formats;
        textures.reserve(images.size());
        texture_formats.reserve(images.size());
        for (const TextureImage* const image : images) {
            if (image == nullptr) {
                textures.push_back(state.white_texture);
                texture_formats.push_back(0);
                continue;
            }
            auto found = state.textures.find(image);
            if (found == state.textures.end()) {
                found =
                    state.textures
                        .emplace(image, std::make_pair(create_texture(*image),
                                                       image->generation))
                        .first;
            } else if (found->second.second != image->generation) {
                glDeleteTextures(1, &found->second.first);
                found->second = { create_texture(*image), image->generation };
            }
            textures.push_back(found->second.first);
            texture_formats.push_back(image->gx_format);
        }

        const Capture capture = read_capture(true);
        /* The clears the frame asked for between its draws, as GXCopyTex made
         * them: a scissored clear of colour and depth before the first run
         * that follows each. */
        const std::size_t clear_count = melee_host_gx_efb_clear_count();
        std::size_t next_clear = 0;
        const auto clear_until = [&](std::size_t triangle) {
            while (next_clear < clear_count) {
                MeleeHostGxEfbClear clear{};
                if (!melee_host_gx_efb_clear_at(next_clear, &clear) ||
                    clear.triangle > triangle)
                {
                    return;
                }
                ++next_clear;
                const melee::gx::WindowRect rect = melee::gx::window_rect(
                    clear.left, clear.top, clear.width, clear.height,
                    framebuffer_width, framebuffer_height, state.width,
                    state.height);
                glEnable(GL_SCISSOR_TEST);
                glScissor(rect.x, rect.y, rect.width, rect.height);
                glDepthMask(GL_TRUE);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                glClearColor(static_cast<float>(clear.color[0]) / 255.0F,
                             static_cast<float>(clear.color[1]) / 255.0F,
                             static_cast<float>(clear.color[2]) / 255.0F,
                             static_cast<float>(clear.color[3]) / 255.0F);
                glClearDepth(static_cast<double>(clear.depth & 0xFFFFFFU) /
                             16777215.0);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }
        };
        glBindVertexArray(state.vertex_array);
        glBindBuffer(GL_ARRAY_BUFFER, state.vertex_buffer);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(capture.vertices.size() * sizeof(float)),
            capture.vertices.data(), GL_STREAM_DRAW);
        for (const DrawRun& run : capture.runs) {
            clear_until(run.first_triangle);
            MeleeHostGxViewState view{};
            if (!melee_host_gx_captured_view_state_at(run.view_state, &view)) {
                continue;
            }
            const melee::gx::WindowRect viewport = melee::gx::window_rect(
                view.viewport_left, view.viewport_top, view.viewport_width,
                view.viewport_height, framebuffer_width, framebuffer_height,
                state.width, state.height);
            glViewport(viewport.x, viewport.y, viewport.width,
                       viewport.height);
            glDepthRange(static_cast<double>(view.viewport_near),
                         static_cast<double>(view.viewport_far));
            /* The host has no GXInit to set a scissor over the whole
             * framebuffer, so a box nothing set is empty, and means no
             * scissor. */
            if (view.scissor_width != 0 && view.scissor_height != 0) {
                const melee::gx::WindowRect scissor = melee::gx::window_rect(
                    static_cast<float>(view.scissor_left),
                    static_cast<float>(view.scissor_top),
                    static_cast<float>(view.scissor_width),
                    static_cast<float>(view.scissor_height), framebuffer_width,
                    framebuffer_height, state.width, state.height);
                glEnable(GL_SCISSOR_TEST);
                glScissor(scissor.x, scissor.y, scissor.width, scissor.height);
            } else {
                glDisable(GL_SCISSOR_TEST);
            }
            /* Front faces wind clockwise in GL's terms through this
             * projection. */
            
            /* Apply widescreen hack (Hor+): squish the projection horizontally
             * in the FBO so the final aspect-correcting blit stretches it out
             * into a wider field of view without distortion. */
            const float target_aspect = ratio(state.video_settings.aspect);
            const float native_aspect = 4.0F / 3.0F;
            if (target_aspect > native_aspect + 0.01F) {
                if (view.projection_type == 0) { /* GX_PERSPECTIVE */
                    view.projection[0] *= (native_aspect / target_aspect);
                } else if (view.projection_type == 1) { /* GX_ORTHOGRAPHIC */
                    /* For UI (ortho), we also squish it so it doesn't stretch, 
                     * and adjust the translation to keep it centered. */
                    view.projection[0] *= (native_aspect / target_aspect);
                    /* Ortho X translation (p1) adjustment to stay centered */
                    view.projection[1] *= (native_aspect / target_aspect); 
                }
            }

            draw_run(run, state.programs.get(), textures, texture_formats,
                     state.white_texture, melee::gx::clip_matrix(view), true);
        }
        clear_until(std::numeric_limits<std::size_t>::max());
        restore_draw_defaults();
    glDisable(GL_SCISSOR_TEST);
    glDepthRange(0.0, 1.0);
        if (!state.hidden) {
            int output_width = 0;
            int output_height = 0;
            SDL_GetWindowSizeInPixels(state.window.window, &output_width,
                                      &output_height);
            if (output_width <= 0 || output_height <= 0) {
                return;
            }
            /* The video menu selects the presentation rectangle.  The game
             * framebuffer is scaled to that rectangle, so a window with the
             * selected aspect fills without letterboxing. */
            const float target_aspect = ratio(state.video_settings.aspect);
            const float output_aspect = static_cast<float>(output_width) /
                                        static_cast<float>(output_height);
            int copy_width = output_width;
            int copy_height = output_height;
            if (output_aspect > target_aspect) {
                copy_width = static_cast<int>(static_cast<float>(output_height) * target_aspect);
            } else {
                copy_height = static_cast<int>(static_cast<float>(output_width) / target_aspect);
            }
            const int copy_left = (output_width - copy_width) / 2;
            const int copy_bottom = (output_height - copy_height) / 2;
            const GLenum blit_filter =
                state.video_settings.filter == PresentationFilter::Linear
                    ? GL_LINEAR : GL_NEAREST;

            /* Helper: blit the FBO to the back-buffer and swap. */
            const auto blit_and_swap = [&]() {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                glViewport(0, 0, output_width, output_height);
                glDisable(GL_SCISSOR_TEST);
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT);
                if (state.target->resolve_framebuffer != 0) {
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, state.target->framebuffer);
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state.target->resolve_framebuffer);
                    glBlitFramebuffer(0, 0, state.width, state.height, 0, 0, state.width, state.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, state.target->resolve_framebuffer);
                } else {
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, state.target->framebuffer);
                }
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                glBlitFramebuffer(0, 0, state.width, state.height,
                                  copy_left, copy_bottom,
                                  copy_left + copy_width,
                                  copy_bottom + copy_height,
                                  GL_COLOR_BUFFER_BIT, blit_filter);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                if (state.video_settings.unlock_requested) {
                    melee_host_game_unlock_all();
                    state.video_settings.unlock_requested = false;
                }
                const VideoSettings previous_settings = state.video_settings;
                if (draw_settings_ui(&state.video_settings, &state.video_menu_open, state.last_fps)) {
                    set_custom_textures_enabled(state.video_settings.custom_textures);
            int aniso = static_cast<int>(state.video_settings.anisotropy);
            if (aniso == 1) g_anisotropy_level = 2;
            else if (aniso == 2) g_anisotropy_level = 4;
            else if (aniso == 3) g_anisotropy_level = 8;
            else if (aniso == 4) g_anisotropy_level = 16;
            else g_anisotropy_level = 0;
                    const bool target_changed =
                        previous_settings.resolution != state.video_settings.resolution ||
                        previous_settings.anti_aliasing != state.video_settings.anti_aliasing;
                    if (target_changed && !configure_render_target(state, nullptr)) {
                        state.video_settings = previous_settings;
                        set_custom_textures_enabled(
                            state.video_settings.custom_textures);
                        const int old_aniso =
                            static_cast<int>(state.video_settings.anisotropy);
                        g_anisotropy_level = old_aniso == 1 ? 2 :
                                             old_aniso == 2 ? 4 :
                                             old_aniso == 3 ? 8 :
                                             old_aniso == 4 ? 16 : 0;
                    } else {
                        if (previous_settings.anisotropy !=
                            state.video_settings.anisotropy) {
                            for (auto& pair : state.textures) {
                                glBindTexture(GL_TEXTURE_2D, pair.second.first);
                                if (SDL_GL_ExtensionSupported(
                                        "GL_EXT_texture_filter_anisotropic")) {
                                    GLfloat maximum = 1.0F;
                                    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,
                                                &maximum);
                                    glTexParameterf(
                                        GL_TEXTURE_2D,
                                        GL_TEXTURE_MAX_ANISOTROPY_EXT,
                                        std::min(static_cast<GLfloat>(
                                                     g_anisotropy_level > 0
                                                         ? g_anisotropy_level : 1),
                                                 maximum));
                                }
                            }
                        }
                        if (previous_settings.window_mode !=
                            state.video_settings.window_mode) {
                            apply_window_mode(state);
                        }
                        save_video_settings(state);
                    }
                }
                SDL_GL_SwapWindow(state.window.window);
            };

            constexpr std::uint64_t kSimTickNs = 1'000'000'000ULL / 60;
            const Uint64 now = SDL_GetTicksNS();
            Uint64& next_sim = state.next_sim_ns;
            if (next_sim == 0 || now > next_sim + kSimTickNs) {
                next_sim = now;
            }
            next_sim += kSimTickNs;

            /* Fixed target rate: pace presentation blits, but still block
             * until the next 60 Hz simulation tick. */
            const std::uint64_t frame_ns =
                1'000'000'000ULL /
                static_cast<std::uint16_t>(state.video_settings.rate);
            Uint64& next_blit = state.next_blit_ns;
            if (next_blit == 0 || now > next_blit + frame_ns) {
                next_blit = now;
            }

            do {
                blit_and_swap();
                double fps = 0.0;
                if (state.frame_rate.add_frame(SDL_GetTicksNS(), &fps)) {
                    state.last_fps = fps;
                }

                next_blit += frame_ns;
                const Uint64 current_time = SDL_GetTicksNS();
                if (next_blit > current_time) {
                    Uint64 delay = next_blit - current_time;
                    /* Never delay past the next simulation tick. */
                    if (current_time + delay > next_sim) {
                        delay = next_sim > current_time ? next_sim - current_time : 0;
                    }
                    if (delay > 0) {
                        SDL_DelayPrecise(delay);
                    }
                }
            } while (SDL_GetTicksNS() < next_sim);
        }
    }

    bool FramePresenter::save_bmp(const char* path, std::string* error)
    {
        std::string ignored;
        std::string& message = error != nullptr ? *error : ignored;
        if (state_ == nullptr || state_->target == nullptr) {
            message = "only a hidden presenter keeps its last frame";
            return false;
        }
        if (!state_->target->resolve_color_buffer()) {
            message = "could not resolve the last frame";
            return false;
        }
        return save_framebuffer_bmp(path, state_->width, state_->height,
                                    &message);
    }

    bool FramePresenter::poll(MeleeHostPadState* pad)
    {
        if (state_ == nullptr) {
            return false;
        }
        State& state = *state_;
        bool running = true;
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN &&
                       event.key.scancode == SDL_SCANCODE_ESCAPE) {
                state.video_menu_open = !state.video_menu_open;
            } else if (process_settings_event(&event)) {
                continue;
            } else if (event.type == SDL_EVENT_GAMEPAD_ADDED &&
                       state.gamepad == nullptr) {
                state.gamepad = SDL_OpenGamepad(event.gdevice.which);
            } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED &&
                       state.gamepad != nullptr &&
                       SDL_GetGamepadID(state.gamepad) == event.gdevice.which) {
                SDL_CloseGamepad(state.gamepad);
                state.gamepad = nullptr;
            }
        }
        /* read_pad also steers the preview's camera, which the game has no use
         * for. */
        float yaw = 0.0F;
        float pitch = 0.0F;
        float zoom = 1.0F;
        const MeleeHostPadState sample =
            state.video_menu_open
                ? MeleeHostPadState{}
                : read_pad(state.gamepad, &yaw, &pitch, &zoom, &running);
        if (pad != nullptr) {
            *pad = sample;
        }
        return running;
    }

    void FramePresenter::pace(std::uint64_t frame_nanoseconds)
    {
        if (state_ == nullptr) {
            return;
        }
        const Uint64 now = SDL_GetTicksNS();
        Uint64& next = state_->next_pace_ns;
        if (next == 0 || now > next + frame_nanoseconds) {
            next = now;
        }
        next += frame_nanoseconds;
        if (next > now) {
            SDL_DelayPrecise(next - now);
        }
    }

    bool FramePresenter::open_audio(std::string* error)
    {
        std::string ignored;
        std::string& message = error != nullptr ? *error : ignored;
        if (state_ == nullptr) {
            message = "the presenter is not open";
            return false;
        }
        if (state_->audio != nullptr) {
            return true;
        }
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            message = SDL_GetError();
            return false;
        }
        const SDL_AudioSpec spec{ SDL_AUDIO_S16, 2, 32000 };
        state_->audio = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (state_->audio == nullptr) {
            message = SDL_GetError();
            return false;
        }
        if (!SDL_ResumeAudioStreamDevice(state_->audio)) {
            message = SDL_GetError();
            return false;
        }
        return true;
    }

    void FramePresenter::queue_audio(const std::int16_t* stereo,
                                     std::uint32_t pairs)
    {
        if (state_ == nullptr || state_->audio == nullptr) {
            return;
        }
        constexpr int kQuarterSecondBytes = 32000 * 4 / 4;
        if (SDL_GetAudioStreamQueued(state_->audio) > kQuarterSecondBytes) {
            return;
        }
        static_cast<void>(SDL_PutAudioStreamData(
            state_->audio, stereo, static_cast<int>(pairs * 4U)));
    }

} // namespace melee::render
