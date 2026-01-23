#include <iostream>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cmath>

std::tuple<int,int> direction_user_should_move()
{
    const bool *key_states = SDL_GetKeyboardState(NULL);
    int x = 0;
    int y = 0;

    /* (We're writing our code such that it sees both keys are pressed and cancels each other out!) */
    if (key_states[SDL_SCANCODE_W]) {
        y += 1;  /* pressed what would be "W" on a US QWERTY keyboard. Move forward! */
    } 

    if (key_states[SDL_SCANCODE_S]) {
        y += -1;  /* pressed what would be "S" on a US QWERTY keyboard. Move backward! */
    }

    if (key_states[SDL_SCANCODE_A]) {
        x += 1;  
    } 

    if (key_states[SDL_SCANCODE_D]) {
        x += -1;  
    } 

    /* (In practice it's likely you'd be doing full directional input in here, but for simplicity, we're just showing forward and backward) */

    return {x,y};  /* wasn't key in W or S location, don't move. */
}

int main(int argc, char* argv[]) {

    SDL_Window *window;                    // Declare a pointer
    bool done = false;

    int width {640};
    int height {480};

    SDL_Init(SDL_INIT_VIDEO);              // Initialize SDL3

    // Create an application window with the following settings:
    window = SDL_CreateWindow(
        "An SDL3 window",                  // window title
        width,                               // width, in pixels
        height,                               // height, in pixels
        SDL_WINDOW_OPENGL                  // flags - see below
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);

    // Check that the window was successfully created
    if (!window || !renderer) {
        // In the case that the window could not be made...
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create window: %s\n", SDL_GetError());
        return 1;
    }

    // Rectangle state
    SDL_FRect rect = { 100.f, 100.f, 100.f, 80.f };

    Uint64 last_counter = SDL_GetPerformanceCounter();
    const double freq = (double)SDL_GetPerformanceFrequency();


    while (!done) {
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (now - last_counter) / freq;
        last_counter = now;

        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                done = true;
            }
        }

        const bool *key_states = SDL_GetKeyboardState(NULL);
        int boost{1};

        (key_states[SDL_SCANCODE_SPACE]) ? boost +=100:boost =1;


        float vx {(100 + boost) * (float) std::get<0>(direction_user_should_move())};
        float vy {(100 + boost) * (float) std::get<1>(direction_user_should_move())};
        
        double direction {-(vx+vy)/sqrt(vx+vy)};

        rect.x -= vx * (float)dt;
        rect.y -= vy * (float)dt;

        if (rect.x<0)
            rect.x = width;
        if (rect.y<0)
            rect.y = height;
        if (rect.x>width)
            rect.x = 0;
        if (rect.y>height)
            rect.y = 0;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Set draw color (red)
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

        // Rectangle (x, y, w, h)
        SDL_RenderFillRect(renderer, &rect);

        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_RenderLine(renderer, rect.x + 0.5*rect.w, rect.y + 0.5*rect.h, rect.x + 0.5*rect.w -vx/10, rect.y + 0.5*rect.h -vy/10);

        // Show result
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    // Close and destroy the window
    SDL_DestroyWindow(window);

    // Clean up
    SDL_Quit();
    return 0;
}