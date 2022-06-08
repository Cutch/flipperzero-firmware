#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#define PIECE_COUNT 7

typedef struct {
    int16_t x;
    int16_t y;
} Offset;

Offset pieceTypes[PIECE_COUNT][4] = {
// x
// x
// x x
    {{0, -1},{0, 0},{0, 1},{1, 1}},

//   x
//   x
// x x
    {{0, -1},{0, 0},{0, 1},{-1, 1}},

//   x
// x x x
    {{0, 1},{0, 0},{-1, 0},{1, 0}},

// x
// x
// x
// x
    {{0, -1},{0, 0},{0, 1},{0, 2}},

// x x
// x x
    {{0, 0},{0, 1},{1, 0},{1, 1}},

// x x
//   x x
    {{-1, 0},{0, 0},{0, 1},{1, 1}},

//   x x
// x x 
    {{-1, 1},{0, 1},{0, 0},{1, 0}},
};


typedef struct {
    int16_t x;
    int16_t y;
    Offset blocks[4];
    uint8_t rotation;
    // PieceType type;

} Piece;

typedef enum {
    GameStateLife,
    GameStateGameOver,
} GameStateOption;

typedef enum {
    None=0,
    Collide,
    PastBottom,
} Collision;

#define PLAY_SIZE 310 // 31*10

typedef struct {
    Piece current_piece;
    Piece next_piece;
    bool existing_pieces[PLAY_SIZE];
    GameStateOption state;
    uint16_t score;
} GameState;

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} GameEvent;

static void falling_blocks_render_callback(Canvas* const canvas, void* ctx) {
    const GameState* game_state = acquire_mutex((ValueMutex*)ctx, 25);
    if(game_state == NULL) {
        return;
    }

    // Before the function is called, the state is set with the canvas_reset(canvas)

    // Frame
    canvas_draw_frame(canvas, 0, 0, 128, 43);

    // Existing blocks
    for(uint16_t i = 0; i < PLAY_SIZE; i++) {
        if(game_state->existing_pieces[i])
            canvas_draw_box(canvas, 123-(uint16_t)(i/10)*4, 38-(i%10)*4, 3, 3);
    }

    // Current piece
    for(uint8_t i = 0; i < 4; i++) {
        Offset block = game_state->current_piece.blocks[i];
        int8_t y = game_state->current_piece.y + block.y;
        int8_t x = game_state->current_piece.x + block.x;
        canvas_draw_box(canvas,
            123-(y*4),
            38-(x*4), 3, 3);
    }

    // Next piece
    for(uint8_t i = 0; i < 4; i++) {
        Offset block = game_state->next_piece.blocks[i];
        int8_t y = block.y + 2;
        int8_t x = block.x + 2;
        canvas_draw_box(canvas,
            24-(y*4),
            60-(x*4), 3, 3);
    }

    canvas_set_font(canvas, FontSecondary);
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "Score: %u", game_state->score);
    canvas_draw_str_aligned(canvas, 128, 64, AlignRight, AlignBottom, buffer);

    // Game Over banner
    if(game_state->state == GameStateGameOver) {
        // Screen is 128x64 px
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 32, 18, 66, 28);

        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 34, 20, 62, 24);
        canvas_draw_frame(canvas, 32, 18, 66, 28);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 37, 31, "Game Over");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 41, AlignCenter, AlignBottom, buffer);
    }

    release_mutex((ValueMutex*)ctx, game_state);
}

static void falling_blocks_input_callback(InputEvent* input_event, osMessageQueueId_t event_queue) {
    furi_assert(event_queue);

    GameEvent event = {.type = EventTypeKey, .input = *input_event};
    osMessageQueuePut(event_queue, &event, 0, osWaitForever);
}

static void falling_blocks_update_timer_callback(osMessageQueueId_t event_queue) {
    furi_assert(event_queue);

    GameEvent event = {.type = EventTypeTick};
    osMessageQueuePut(event_queue, &event, 0, 0);
}


static void falling_blocks_get_new_block(GameState* const game_state) {
    // Offset blocks[4];
    Piece p = {.x = 0, .y = 0, .rotation = 0};
    memcpy(p.blocks, pieceTypes[rand() % PIECE_COUNT], sizeof(Offset)*4);
    game_state->current_piece = game_state->next_piece;
    game_state->current_piece.x = 5;
    game_state->current_piece.y = 31;

    game_state->next_piece = p;
}

static void falling_blocks_init_game(GameState* const game_state) {
    memset(game_state->existing_pieces, false, sizeof(bool)*PLAY_SIZE);

    game_state->state = GameStateLife;

    Piece next_piece = {.x = 0, .y = 0, .rotation = 0};
    Piece current_piece = {.x = 0, .y = 0, .rotation = 0};
    game_state->next_piece = next_piece;
    game_state->current_piece = current_piece;

    falling_blocks_get_new_block(game_state);
    falling_blocks_get_new_block(game_state);
}

static Collision falling_blocks_block_will_collide(GameState* const game_state, Offset offset) {
    bool pastBottom = false;
    for(uint8_t i = 0; i < 4; i++) {
        Offset block = game_state->current_piece.blocks[i];
        int16_t y = game_state->current_piece.y + block.y + offset.y;
        int16_t x = game_state->current_piece.x + block.x + offset.x;
        if(x >= 10 || x < 0 || (y*10+x < PLAY_SIZE && game_state->existing_pieces[y*10+x])) { 
            return Collide;
        }
        if(y < 0) pastBottom = true;
    }
    if(pastBottom)
        return PastBottom;
    return None;
}

static void falling_blocks_block_move(GameState* const game_state, Offset offset) {
    game_state->current_piece.x += offset.x;
    if(game_state->current_piece.y != 0) game_state->current_piece.y += offset.y;
}

static void _falling_blocks_block_rotate(GameState* const game_state, Offset oldBlocks[], int8_t direction) {
    if(direction == 1) {
        game_state->current_piece.rotation = (game_state->current_piece.rotation+1)%4;
        for(uint8_t i = 0; i < 4; i++) {
            game_state->current_piece.blocks[i].x = oldBlocks[(i+1)%4].y;
            game_state->current_piece.blocks[i].y = oldBlocks[(i+1)%4].x;

            if(game_state->current_piece.rotation % 2 == 1){
                game_state->current_piece.blocks[i].x *= -1;
                game_state->current_piece.blocks[i].y *= -1;
            }
        }
    } else {
        game_state->current_piece.rotation = (game_state->current_piece.rotation-1+4)%4;
        for(uint8_t i = 0; i < 4; i++) {
            game_state->current_piece.blocks[i].x = oldBlocks[(i-1+4)%4].y;
            game_state->current_piece.blocks[i].y = oldBlocks[(i-1+4)%4].x;

            if(game_state->current_piece.rotation % 2 == 0){
                game_state->current_piece.blocks[i].x *= -1;
                game_state->current_piece.blocks[i].y *= -1;
            }
        }
    }
}
static void falling_blocks_block_rotate(GameState* const game_state, int8_t direction) {
    Offset oldBlocks[4];
    memcpy(oldBlocks, game_state->current_piece.blocks, sizeof(Offset)*4);

    uint16_t oldX = 0;
    for(uint8_t i = 0; i < 4; i++) {
        oldX += oldBlocks[i].x;
    }

    _falling_blocks_block_rotate(game_state, oldBlocks, direction);

    uint16_t newX = 0;
    for(uint8_t i = 0; i < 4; i++) {
        newX += game_state->current_piece.blocks[i].x;
    }

    Offset o = {.x=(newX > oldX ? -1 : 0), .y=0};

    bool fixed = false;
    // Check for collision
    Collision c;

    if((c = falling_blocks_block_will_collide(game_state, o))){
        if(c == PastBottom) game_state->current_piece.y += 1; // Push it up if its past the bottom
        for(uint8_t xOffsetCheck = 1; !fixed && xOffsetCheck <= 3; xOffsetCheck++){
            o.x = (newX > oldX ? -1 : 1) * xOffsetCheck;
            if((c = falling_blocks_block_will_collide(game_state, o))){
                if(c == PastBottom) game_state->current_piece.y += 1; // Push it up if its past the bottom
                falling_blocks_block_move(game_state, o);
                fixed = true;
            }else{
                o.x *= -1;
                if((c = falling_blocks_block_will_collide(game_state, o))){
                    if(c == PastBottom) game_state->current_piece.y += 1; // Push it up if its past the bottom
                    falling_blocks_block_move(game_state, o);
                    fixed = true;
                }
            }
        }
        if(!fixed){
            // Revert the rotation if it cant be turned
            _falling_blocks_block_rotate(game_state, oldBlocks, -direction);
        }
    }
    // memcpy(game_state->current_piece.blocks, newBlocks, sizeof(Offset)*4);
}

static void falling_blocks_block_shift(GameState* const game_state, Offset offset) {
    if(!falling_blocks_block_will_collide(game_state, offset)){
        falling_blocks_block_move(game_state, offset);
    }
}

static void falling_blocks_shift_rows(GameState* const game_state, uint8_t deleteY) {
    for(uint8_t y = deleteY; y < 31; y++) {
        for(uint8_t x = 0; x < 10; x++) {
            if(y + 1 < 31)
                game_state->existing_pieces[y*10+x] = game_state->existing_pieces[(y+1)*10+x];
            else
                game_state->existing_pieces[y*10+x] = false;
        }
    }
}

static void falling_blocks_check_rows(GameState* const game_state) {
    for(uint8_t y = 0; y < 31; y++) {
        bool full = true;
        for(uint8_t x = 0; x < 10; x++) {
            full = full && game_state->existing_pieces[y*10+x];
        }
        if(full){
            falling_blocks_shift_rows(game_state, y);
            game_state->score += 10;
        }
    }
}

static void falling_blocks_process_game_step(GameState* const game_state) {
    if(game_state->state == GameStateGameOver) {
        return;
    }
    Offset o = {.x=0, .y=-1};

    // Check for collision
    if(falling_blocks_block_will_collide(game_state, o)){
        // Finalize block place
        for(uint8_t i = 0; i < 4; i++) {
            Offset block = game_state->current_piece.blocks[i];
            int16_t y = game_state->current_piece.y + block.y;
            int16_t x = game_state->current_piece.x + block.x;
            game_state->existing_pieces[y*10+x] = true;
        }
        game_state->score++;
        falling_blocks_check_rows(game_state);
        falling_blocks_get_new_block(game_state);
        Offset x = {.x=0, .y=0};
        // Check for end game collision
        if(falling_blocks_block_will_collide(game_state, x)){
            game_state->state = GameStateGameOver;
        }
    }else{
        // Fall
        falling_blocks_block_move(game_state, o);
    }
}

int32_t falling_blocks_app(void* p) {
    UNUSED(p);
    srand(DWT->CYCCNT);

    osMessageQueueId_t event_queue = osMessageQueueNew(8, sizeof(GameEvent), NULL);

    GameState* game_state = malloc(sizeof(GameState));
    falling_blocks_init_game(game_state);

    ValueMutex state_mutex;
    if(!init_mutex(&state_mutex, game_state, sizeof(GameState))) {
        FURI_LOG_E("FallingBlocksGame", "cannot create mutex\r\n");
        free(game_state);
        return 255;
    }

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, falling_blocks_render_callback, &state_mutex);
    view_port_input_callback_set(view_port, falling_blocks_input_callback, event_queue);

    osTimerId_t timer =
        osTimerNew(falling_blocks_update_timer_callback, osTimerPeriodic, event_queue, NULL);
    osTimerStart(timer, osKernelGetTickFreq() * 0.2);

    // Open GUI and register view_port
    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    Offset right = {.x=1, .y=0};
    Offset left = {.x=-1, .y=0};
    Offset down = {.x=0, .y=-1};

    GameEvent event;
    for(bool processing = true; processing;) {
        osStatus_t event_status = osMessageQueueGet(event_queue, &event, NULL, 100);

        GameState* game_state = (GameState*)acquire_mutex_block(&state_mutex);

        if(event_status == osOK) {
            // press events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                    case InputKeyUp:
                        falling_blocks_block_shift(game_state, right);
                        break;
                    case InputKeyDown:
                        falling_blocks_block_shift(game_state, left);
                        break;
                    case InputKeyRight:
                        falling_blocks_block_shift(game_state, down);
                        break;
                    case InputKeyLeft:
                        falling_blocks_block_rotate(game_state, -1);
                        break;
                    case InputKeyOk:
                        if(game_state->state == GameStateGameOver) {
                            falling_blocks_init_game(game_state);
                        }else{
                            falling_blocks_block_rotate(game_state, 1);
                        }
                        break;
                    case InputKeyBack:
                        processing = false;
                        break;
                    }
                }
            } else if(event.type == EventTypeTick) {
                falling_blocks_process_game_step(game_state);
            }
        } else {
            // event timeout
        }

        view_port_update(view_port);
        release_mutex(&state_mutex, game_state);
    }

    osTimerDelete(timer);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close("gui");
    view_port_free(view_port);
    osMessageQueueDelete(event_queue);
    delete_mutex(&state_mutex);
    free(game_state);

    return 0;
}

// Screen is 128x64 px
// (4 + 4) * 16 - 4 + 2 + 2border == 128
// (4 + 4) * 8 - 4 + 2 + 2border == 64
// Game field from point{x:  0, y: 0} to point{x: 30, y: 14}.
// The snake turns only in even cells - intersections.
// ┌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┐
// ╎ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ╎
// ╎ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ╎
// ╎ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ╎
// ╎ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ╎
// ╎ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ╎
// ╎ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ╎
// ╎ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ╎
// ╎ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ╎
// ╎ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ╎
// ╎ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ▪ ╎
// ╎                                                               ╎
// ╎   ▪ ▪ ▪                                                       ╎
// ╎   ▪ ▪ ▪                                                       ╎
// ╎   ▪ ▪ ▪                                                       ╎
// ╎                                                               ╎
// └╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┘
