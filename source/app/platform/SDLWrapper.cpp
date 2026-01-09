#include "StdInc.h"

#ifdef NOTSA_USE_SDL3
#include <SDL3/SDL.h>
#include "SDLWrapper.hpp"
#include <bindings/imgui_impl_sdl3.h>
#include <WindowedMode.hpp>
#include "PostEffects.h"
#include "UIRenderer.h"

namespace notsa {
namespace SDLWrapper {
bool Initialize() {
    return true;
}

void Terminate() {

}

// SEH wrapper to safely poll events (catches MSCTF exceptions in VMs)
// Must be in separate function because SEH can't be used with C++ objects that have destructors
static bool SafePollEvent(SDL_Event* e) {
    __try {
        return SDL_PollEvent(e);
    } __except (GetExceptionCode() == 0xe06d7363 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        // Ignore C++ exceptions from MSCTF (Text Services Framework) in VMs
        return false;
    }
}

void ProcessEvents() {
    // Now process events
    const auto* const imCtx = ImGui::GetCurrentContext();
    const auto* const imIO  = imCtx ? &imCtx->IO : nullptr;
    // Check if SDL3 backend is initialized (BackendPlatformUserData is set by ImGui_ImplSDL3_InitForD3D)
    const bool imSdl3BackendInitialized = imIO && imIO->BackendPlatformUserData;
    SDL_Event e;
    while (SafePollEvent(&e)) {
        if (imSdl3BackendInitialized) {
            ImGui_ImplSDL3_ProcessEvent(&e);
        }

        switch (e.type) {
        case SDL_EVENT_QUIT: {
            RsGlobal.quit = true;
            continue;
        }
        case SDL_EVENT_WINDOW_RESIZED: {
            const auto w = e.window.data1,
                       h = e.window.data2;

            NOTSA_LOG_DEBUG(
                "SDL: Window resized: {} x {}",
                w, h
            );

            continue;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            if (notsa::ui::UIRenderer::HasInstance() && notsa::ui::UIRenderer::GetSingleton().IsActive()) {
                break;
            }
            static CVector2D s_MousePos{};
            if (FrontEndMenuManager.m_bMenuActive) {
                s_MousePos.x += e.motion.xrel * CCamera::m_fMouseAccelHorzntl * 100.f;
                s_MousePos.y += e.motion.yrel * CCamera::m_fMouseAccelVertical * 100.f;
            } else {
                s_MousePos.x = e.motion.x;
                s_MousePos.y = e.motion.y;
            }
            FrontEndMenuManager.m_nMousePosWinX = (int32)(s_MousePos.x);
            FrontEndMenuManager.m_nMousePosWinY = (int32)(s_MousePos.y);
            break;
        }
        }

        if (CPad::ProcessEvent(e, imIO && imIO->WantCaptureMouse, imIO && imIO->WantCaptureKeyboard)) {
            continue;
        }

        //NOTSA_LOG_DEBUG("SDL: Unprocessed event: {}", e.type);
    }
}
}; // namespace SDLWrapper
}; // namespace notsa
#endif
