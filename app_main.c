#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <furi_hal_power.h>
#include <core/thread.h>
#include <storage/storage.h>

/* generated by fbt from .png files in images folder */
#include <charge_controller_icons.h>

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriThread* reader_thread;
    FuriMessageQueue* event_queue;
} ExampleGuiContext;

void power_app_draw_callback(Canvas* canvas, void* ctx);
void power_app_input_callback(InputEvent* event, void* ctx);
int32_t power_app_reader_thread_callback(void* ctx);

#define ReaderThreadFlagExit 1

static ExampleGuiContext* power_app_context_alloc() {
    ExampleGuiContext* context = malloc(sizeof(ExampleGuiContext));
    // ViewPortを初期化する
    context->view_port = view_port_alloc();
    view_port_draw_callback_set(context->view_port, power_app_draw_callback, context);
    view_port_input_callback_set(context->view_port, power_app_input_callback, context);

    context->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    context->reader_thread = furi_thread_alloc();
    furi_thread_set_stack_size(context->reader_thread, 1024U);
    furi_thread_set_context(context->reader_thread, context);
    furi_thread_set_callback(context->reader_thread, power_app_reader_thread_callback);
    // GUIを取得する
    context->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(context->gui, context->view_port, GuiLayerFullscreen);
    return context;
}
void power_app_input_callback(InputEvent* event, void* ctx) {
    ExampleGuiContext* context = ctx;
    furi_message_queue_put(context->event_queue, event, FuriWaitForever);
}

// static char* STATE_FILE = APP_DATA_PATH("power_app_state.state");

#define TAG "power_app"
void power_app_run(ExampleGuiContext* context) {
    /* メッセージループ */
    for(bool is_running = true; is_running;) {
        InputEvent event;
        /* Wait for an input event. Input events come from the GUI thread via a callback. */
        const FuriStatus status =
            furi_message_queue_get(context->event_queue, &event, FuriWaitForever);

        /* This application is only interested in short button presses. */
        if((status != FuriStatusOk) || (event.type != InputTypeShort)) {
            continue;
        }

        /* キーイベントをキャッチしていい感じに処理する */
        if(event.key == InputKeyBack) {
            is_running = false;
        }
        if(event.key == InputKeyRight) {
            furi_hal_power_off();
        }
        if(event.key == InputKeyOk) {
            furi_hal_power_suppress_charge_enter();
        }
        if(event.key == InputKeyLeft) {
            furi_hal_power_suppress_charge_exit();
        }
    }
}
#define TEXT_STORE_SIZE 64
#define UPDATE_PERIOD_MS 1000UL
void power_app_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    const size_t middle_x = canvas_width(canvas) / 2U;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, middle_x, 12, AlignCenter, AlignBottom, "Charge Controller");
    canvas_draw_line(canvas, 0, 16, 128, 16);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 0, 30, AlignLeft, AlignBottom, "Left: charge");
    canvas_draw_str_aligned(canvas, 0, 30 + 8, AlignLeft, AlignBottom, "Center: suppress charge");
    canvas_draw_str_aligned(canvas, 0, 30 + 8 + 8, AlignLeft, AlignBottom, "Right: power off");
    canvas_draw_str_aligned(canvas, middle_x, 64, AlignCenter, AlignBottom, "Author: @segfo");
}

int32_t power_app_reader_thread_callback(void* ctx) {
    // ExampleGuiContext* context = ctx;
    UNUSED(ctx);

    for(;;) {
        /* Wait for the measurement to finish. At the same time wait for an exit signal. */
        const uint32_t flags =
            furi_thread_flags_wait(ReaderThreadFlagExit, FuriFlagWaitAny, UPDATE_PERIOD_MS);

        /* If an exit signal was received, return from this thread. */
        if(flags != (unsigned)FuriFlagErrorTimeout) break;
    }

    return 0;
}

/* Release the unused resources and deallocate memory */
static void power_app_context_free(ExampleGuiContext* context) {
    view_port_enabled_set(context->view_port, false);
    gui_remove_view_port(context->gui, context->view_port);

    furi_thread_free(context->reader_thread);
    furi_message_queue_free(context->event_queue);
    view_port_free(context->view_port);

    furi_record_close(RECORD_GUI);
}

int32_t flipper_main(void* p) {
    UNUSED(p);
    ExampleGuiContext* context = power_app_context_alloc();
    power_app_run(context);
    power_app_context_free(context);
    return 0;
}
