#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <math.h>

#include "parse.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define PORT 12345

#define TARGET_FPS 120
#define FRAME_TIME (1000.0f / TARGET_FPS)
#define GRAVITY 9.8f
#define FRICTION 0.999f // slowly decrease velocity

typedef struct {
    float x, y;
} Position;

// ball struct
typedef struct {
    float centerX, centerY;
    float vx, vy;           // velocity in pixels/sec
    float radius;
    SDL_Color color;
} Ball;

void DrawFilledCircle(SDL_Renderer *renderer, int cx, int cy, int radius);
void grav_updateBallPos(Ball *ball, float deltaTime);
void grav_updateBallPhys(Ball *ball, float deltaTime);
void HandleCollision(Ball *ball, Position beamCenter, float beamWidth, float beamHeight, float beamAngleDegrees, float deltaTime);

int main() 
{
    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return 1;
    }

    // make socket non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl failed");
        return 1;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = PF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY; // listen on all interfaces
    servaddr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        return 1;
    }

    // recieve data from phone 
    char recv_buffer[4096];
    char json_accum[65536];
    size_t accum_len = 0;

    // SDL start
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    SDL_Event event;

    // init SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldnt initialise SDL: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    window = SDL_CreateWindow("Gyro Control", 960, 540, 0);
    renderer = SDL_CreateRenderer(window, NULL);

    if (window == NULL || renderer == NULL) {
        SDL_Log("Couldnt create window || renderer: %s\n", SDL_GetError());
        return 1;
    }

    // create texture
    SDL_Texture* beamTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, 1, 1);
    Uint32 white = 0xFFFFFFFF;
    SDL_UpdateTexture(beamTexture, NULL, &white, 4);

    // beam properties
    float beamAngle = 0.0f;
    Position beamCenter = {480.0f, 400.0f}; // center screen
    float beamWidth = 500.0f;
    float beamHeight = 10.0f;

    Ball ball = {480.0f, 200.0f, 0.0f, 0.0f, 20.0f, {255, 0, 0, 255}};

    Uint64 lastTime = SDL_GetTicks();

    bool running = true;


    printf("Server listening on port %d...\n", PORT);
    
    // main loop
    while (running) {
        // SDL event handle

        Uint64 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        // attempt to recieve UDP data (non blocking will return immeadeatly)
        ssize_t received = recvfrom(sockfd, recv_buffer, sizeof(recv_buffer) - 1, 0, NULL, NULL);
        
        if (received > 0) {
            recv_buffer[received] = '\0';

        // append to accumulator
        if (accum_len + received >= sizeof(json_accum)) {
            printf("[WARN] Accumulator overflow, resetting.\n");
            accum_len = 0;
            continue;
        }

        memcpy(json_accum + accum_len, recv_buffer, received);
        accum_len += received;
        json_accum[accum_len] = '\0';

        // parse only complete JSON objects
        char *start = json_accum;
        char *end;

        while ((end = strchr(start, '}'))) {
            *(end + 1) = '\0';  // temporarily terminate at end of JSON

            MotionData data = parse_motion_data(start);

            printf("Pitch: %.6f | Roll: %.6f\n",
                data.pitch, data.roll);

            // adjust x and y vals with data (multipliers control sensetivity)
            beamAngle = data.roll * 57.2958f; // convert from radians to degrees

            // move start past this JSON object
            start = end + 1;
        }

        // move leftover incomplete data to start of accumulator
        size_t leftover = strlen(start);
        memmove(json_accum, start, leftover);
        accum_len = leftover;
        json_accum[accum_len] = '\0';
        }
        else if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("recvfrom error");
        }

        // rendering
        SDL_SetRenderDrawColor(renderer, 0,0,0,255);
        SDL_RenderClear(renderer);

        SDL_FRect beamRect = {
            beamCenter.x - (beamWidth / 2.0f),
            beamCenter.y - (beamHeight / 2.0f),
            beamWidth,
            beamHeight
        };

        SDL_SetRenderDrawColor(renderer, ball.color.r, ball.color.g, ball.color.b, ball.color.a);
        DrawFilledCircle(renderer, ball.centerX, ball.centerY, ball.radius);

        // define pivot point
        SDL_FPoint pivot = { beamWidth / 2.0f, beamHeight / 2.0f };

        SDL_SetTextureColorMod(beamTexture, 255, 255, 255); // white
        SDL_RenderTextureRotated(renderer, beamTexture, NULL, &beamRect, (double)beamAngle, &pivot, SDL_FLIP_NONE);

        grav_updateBallPhys(&ball, deltaTime);
        grav_updateBallPos(&ball, deltaTime);

        HandleCollision(&ball, beamCenter, beamWidth, beamHeight, beamAngle, deltaTime);

        SDL_RenderPresent(renderer);

        // cap framrate to 60, change UDP polling to 60hz also (located in mobile device setting)
        SDL_Delay(16);
        }

    // clean
    SDL_DestroyTexture(beamTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    close(sockfd);

    return 0;
}
 

void DrawFilledCircle(SDL_Renderer *renderer, int cx, int cy, int radius) {
    int x = radius;
    int y = 0;
    int decision = 1 - x;

    while (y <= x ) {
        SDL_RenderLine(renderer, cx - x, cy + y, cx + x, cy + y);
        SDL_RenderLine(renderer, cx - x, cy - y, cx + x, cy - y);
        SDL_RenderLine(renderer, cx - y, cy + x, cx + y, cy + x);
        SDL_RenderLine(renderer, cx - y, cy - x, cx + y, cy - x);

        y++;

        if (decision <= 0)
            decision += 2 * y + 1;
        else {
            x--;
            decision += 2 * (y-x) + 1;
        }
    }
}

void grav_updateBallPos(Ball *ball, float deltaTime) {
    ball->centerX += ball->vx * deltaTime;
    ball->centerY += ball->vy * deltaTime;
}

void grav_updateBallPhys(Ball *ball, float deltaTime) {
    ball->vx *= FRICTION;
    ball->vy += GRAVITY * 100 * deltaTime;
}

void HandleCollision(Ball *ball, Position beamCenter, float beamWidth, float beamHeight, float beamAngleDegrees, float deltaTime) {
    float angleRad = beamAngleDegrees * (M_PI / 180.0f);

    // translate ball relative to beams center
    float dx = ball->centerX - beamCenter.x;
    float dy = ball->centerY - beamCenter.y;

    // otate ball to 'local space'
    float localX = dx * cosf(-angleRad) - dy * sinf(-angleRad);
    float localY = dx * sinf(-angleRad) + dy * cosf(-angleRad);

    float halfW = beamWidth / 2.0f;
    float halfH = beamHeight / 2.0f;

    // Check if ball is within bounds of the beam (AABB check)
    if (localX > -halfW && localX < halfW) {
        // Check if ball is hitting the top of the beam
        if (localY + ball->radius > -halfH && localY < halfH) {
            
            // reposition ball to sit exactly on the surface in local space
            localY = -halfH - ball->radius;

            // --- PHYSICS START ---
            // Calculate how much the ball should "slide" based on the tilt
            // We use sin(angle) because at 0 degrees, force is 0. At 90, force is max.
            float slopeAcceleration = GRAVITY * 150.0f * sinf(angleRad);
            
            // Apply this force to the horizontal velocity
            ball->vx += slopeAcceleration * deltaTime; // approx deltaTime
            
            // Kill vertical velocity so it doesn't "bounce" or vibrate while sitting
            ball->vy = 0; 
            // --- PHYSICS END ---

            // 4. Convert local pos back to global space so it renders correctly
            ball->centerX = beamCenter.x + (localX * cosf(angleRad) - localY * sinf(angleRad));
            ball->centerY = beamCenter.y + (localX * sinf(angleRad) + localY * cosf(angleRad));
        }
    }
}
