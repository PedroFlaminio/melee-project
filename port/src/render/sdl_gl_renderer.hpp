#ifndef MELEE_HOST_SDL_GL_RENDERER_HPP
#define MELEE_HOST_SDL_GL_RENDERER_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <melee_host/input.h>

struct MeleeHostContext;

namespace melee::render {

    struct TextureImage {
        std::uint16_t width;
        std::uint16_t height;
        std::uint32_t wrap_s;
        std::uint32_t wrap_t;
        std::vector<std::uint8_t> rgba;
        /* The texture asked for bilinear magnification.  Mipmaps are not
         * decoded, so minification follows the same choice. */
        bool linear_filter;
        bool mipmap;
        /* Bumped when the image is decoded again at the same address, as an
         * EFB copy the game makes every frame is; the presenter uploads it
         * again. */
        std::uint32_t generation = 0;
        /* GX texture format.  Depth formats store their original depth bytes
         * in rgba so the presenter can reconstruct gl_FragDepth. */
        std::uint32_t gx_format = 0;
    };

    /* Images in the order of the captured texture table, which is the space
     * the captured texture sets index. */
    void set_texture_images(std::vector<TextureImage> images);

    /* Called once per displayed frame, before the geometry is read.  A viewer
     * that wants motion advances the animation and captures again from here,
     * which is what makes the window show a moving model instead of a still
     * one. */
    using FrameCallback = void (*)(void* user_data);

    /* Shows the captured geometry, each draw coloured by its TEV program
     * evaluated per fragment.  When the MELEE_HOST_SCREENSHOT environment
     * variable names a path, one frame is rendered off screen into that BMP
     * instead and the window never opens. */
    [[nodiscard]] bool show_captured_geometry(MeleeHostContext* context,
                                              std::string* error,
                                              FrameCallback on_frame = nullptr,
                                              void* user_data = nullptr);

    /* A window the game's own frame loop presents into.  present() draws one
     * frame of the GX capture with the projection, viewport and scissor each
     * draw ran under, in the order the game drew, over the clear colour and
     * depth of the display copy.  Call it from the GX frame sink, while the
     * capture is still there. */
    class FramePresenter {
    public:
        struct State;

        FramePresenter();
        ~FramePresenter();
        FramePresenter(const FramePresenter&) = delete;
        FramePresenter& operator=(const FramePresenter&) = delete;

        /* A hidden presenter draws into an offscreen target of the game's
         * framebuffer size and never shows a window. */
        [[nodiscard]] bool open(bool hidden, std::string* error);
        /* `images` holds one image per captured texture id of this frame, or
         * nullptr for one that could not be decoded.  A GL texture is kept per
         * image address, so an image must outlive the presenter and stay
         * unchanged. */
        void present(const std::vector<const TextureImage*>& images);
        /* Writes the last frame a hidden presenter drew to a BMP file. */
        [[nodiscard]] bool save_bmp(const char* path, std::string* error);
        /* Handles window events and reads the keyboard and the first gamepad
         * as one PAD.  Returns false once the window is closed, Escape is
         * pressed or the gamepad's Back button is held. */
        [[nodiscard]] bool poll(MeleeHostPadState* pad);
        /* Sleeps until one frame period has passed since the previous call.  A
         * presenter that fell behind starts counting again instead of catching
         * up. */
        void pace(std::uint64_t frame_nanoseconds);
        /* Opens the default playback device for the AX mixer's output, 16-bit
         * stereo at 32 kHz.  Without it the game plays silently. */
        [[nodiscard]] bool open_audio(std::string* error);
        /* Queues one batch of the mixer's stereo pairs.  A batch that finds
         * the device more than a quarter second behind is dropped, so a device
         * clock slower than the game's cannot build up delay. */
        void queue_audio(const std::int16_t* stereo, std::uint32_t pairs);

    private:
        std::unique_ptr<State> state_;
    };

    struct TevConformanceReport {
        std::size_t programs = 0;
        std::size_t cases = 0;
        std::size_t mismatches = 0;
        std::string first_mismatch;
    };

    /* Holds the generated shaders to the CPU reference: every captured pair of
     * TEV program and pixel state is rendered into a one-pixel target with
     * random rasterized colours and texels, and the pixel read back must equal
     * what melee::gx::evaluate_tev and the alpha test compute, discards
     * included. Needs a GL context but never shows a window. */
    [[nodiscard]] bool run_tev_conformance(std::size_t cases_per_program,
                                           TevConformanceReport* report,
                                           std::string* error);

} // namespace melee::render

#endif
