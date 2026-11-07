#include <stdint.h>
#include <string.h>

#include "../output_common/render_method.h"

#ifdef RENDER_METHOD_LCD

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_cpu.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <rom/cache.h>

#include "../epd_internals.h"
#include "../output_common/line_queue.h"
#include "../output_common/lut.h"
#include "../output_common/render_context.h"
#include "epd_board.h"
#include "epdiy.h"
#include "lcd_driver.h"
#include "render_lcd.h"
#include "../output_common/quality_regions.h"

// declare vector optimized line mask application.
void epd_apply_line_mask_VE(uint8_t* line, const uint8_t* mask, int mask_len);

static struct {
    int first_line,phase,thread,assigned,current,last,other_current,other_last;
    uint32_t max_cycles[NUM_RENDER_THREADS];
    unsigned rendered[NUM_RENDER_THREADS];
    int wait_timeout_row[NUM_RENDER_THREADS];
} feed_diag;

__attribute__((optimize("O3"))) static bool IRAM_ATTR
retrieve_line_isr(RenderContext_t* ctx, uint8_t* buf) {
    if (ctx->lines_consumed >= ctx->lines_total) {
        memset(buf, 0x00, ctx->display_width / 4);
        return false;
    }
    int thread = ctx->line_threads[ctx->lines_consumed];
    BaseType_t awoken = pdFALSE;

    if (thread >= NUM_RENDER_THREADS) {
        ctx->error |= EPD_DRAW_EMPTY_LINE_QUEUE;
        memset(buf, 0x00, ctx->display_width / 4);
        ctx->lines_consumed += 1;
        return awoken;
    }

    LineQueue_t* lq = &ctx->line_queues[thread];

    if (lq_read(lq, buf) != 0) {
        if(feed_diag.first_line<0) {
            feed_diag.first_line=ctx->lines_consumed;feed_diag.phase=ctx->current_frame;
            feed_diag.thread=thread;feed_diag.assigned=atomic_load(&ctx->lines_prepared);
            feed_diag.current=atomic_load(&lq->current);feed_diag.last=atomic_load(&lq->last);
            feed_diag.other_current=atomic_load(&ctx->line_queues[!thread].current);
            feed_diag.other_last=atomic_load(&ctx->line_queues[!thread].last);
        }
        ctx->error |= EPD_DRAW_EMPTY_LINE_QUEUE;
        memset(buf, 0x00, ctx->display_width / 4);
    }

    if (ctx->lines_consumed >= ctx->display_height) {
        memset(buf, 0x00, ctx->display_width / 4);
    }
    ctx->lines_consumed += 1;
    return awoken;
}

/// start the next frame in the current update cycle
static void IRAM_ATTR handle_lcd_frame_done(RenderContext_t* ctx) {
    epd_lcd_frame_done_cb(NULL, NULL);
    epd_lcd_line_source_cb(NULL, NULL);

    // 扫描先于供数结束时置错，禁止残留行进入下一相位
    extern bool epd_lcd_frame_failed(void);
    if (ctx->lines_consumed < ctx->lines_total || epd_lcd_frame_failed()) {
        ctx->error |= EPD_DRAW_EMPTY_LINE_QUEUE;
    }

    BaseType_t task_awoken = pdFALSE;
    xSemaphoreGiveFromISR(ctx->frame_done, &task_awoken);

    portYIELD_FROM_ISR();
}

void lcd_do_update(RenderContext_t* ctx) {
    memset(&feed_diag,0,sizeof(feed_diag));feed_diag.first_line=-1;
    for(int i=0;i<NUM_RENDER_THREADS;++i)feed_diag.wait_timeout_row[i]=-1;
    int64_t period_min=INT64_MAX,period_max=0,period_sum=0;
    int64_t prior_start=0,cadence_min=INT64_MAX,cadence_max=0,cadence_sum=0;
    unsigned cadence_n=0;
    unsigned completed=0;
    epd_set_mode(1);
    ESP_LOGD("epd_lcd", "update %d frames, lines=%d (LCD frame-end paced)", ctx->cycle_frames,
             ctx->lines_total);

    for (uint8_t k = 0; k < ctx->cycle_frames; k++) {
        epd_lcd_frame_done_cb((frame_done_func_t)handle_lcd_frame_done, ctx);
        prepare_context_for_next_frame(ctx);
        wordcard_quality_prepare_phase(ctx->current_frame);

        const int64_t t0 = esp_timer_get_time();

        // start both feeder tasks；开扫由 feeder 预填够行后触发
        xTaskNotifyGive(ctx->feed_tasks[!xPortGetCoreID()]);
        xTaskNotifyGive(ctx->feed_tasks[xPortGetCoreID()]);

        if (xSemaphoreTake(ctx->frame_done, pdMS_TO_TICKS(15000)) != pdTRUE) {
            ESP_LOGE("epd_lcd", "LCD frame %d/%d timeout", k + 1, ctx->cycle_frames);
            ctx->error |= EPD_DRAW_EMPTY_LINE_QUEUE;
            epd_lcd_line_source_cb(NULL, NULL);
            epd_lcd_abort_frame();
            xSemaphoreTake(ctx->frame_done, 0);
        }

        for (int i = 0; i < NUM_RENDER_THREADS; i++) {
            if (xSemaphoreTake(ctx->feed_done_smphr[i], pdMS_TO_TICKS(15000)) != pdTRUE) {
                ESP_LOGE("epd_lcd", "feed %d timeout @frame %d", i, k);
                ctx->error |= EPD_DRAW_EMPTY_LINE_QUEUE;
            }
        }

        // CKV width, line period and start-to-start cadence are separate
        // quantities. The TFT holds pixel charge between selections; do not
        // equate the gate's high time or the feeder loop with pulse energy.
        const int64_t scan_start=epd_lcd_frame_started_us();
        if(prior_start>0&&scan_start>prior_start) {
            const int64_t dt=scan_start-prior_start;
            if(dt<cadence_min)cadence_min=dt;
            if(dt>cadence_max)cadence_max=dt;
            cadence_sum+=dt;++cadence_n;
        }
        prior_start=scan_start;
        vTaskDelay(0);

        const int64_t period_us = esp_timer_get_time() - t0;
        if(period_us<period_min)period_min=period_us;
        if(period_us>period_max)period_max=period_us;
        period_sum+=period_us;++completed;
        if ((k & 3) == 0 || k + 1 == ctx->cycle_frames) {
            ESP_LOGD("epd_lcd", "phase %d/%d period=%lldus", k + 1, ctx->cycle_frames,
                     (long long)period_us);
        }

        if (ctx->error) {
            for (int i = 0; i < NUM_RENDER_THREADS; i++) {
                lq_reset(&ctx->line_queues[i]);
            }
            break;
        }

        ctx->current_frame++;
    }

    epd_lcd_line_source_cb(NULL, NULL);
    epd_lcd_frame_done_cb(NULL, NULL);

    epd_set_mode(0);
    ESP_LOGI("wordcard_scan","PHASES completed=%u/%u min=%lld avg=%lld max=%lld us error=%u",
        completed,ctx->cycle_frames,(long long)(completed?period_min:0),
        (long long)(completed?period_sum/completed:0),(long long)period_max,(unsigned)ctx->error);
    extern void epd_lcd_log_scan_stats(void);
    epd_lcd_log_scan_stats();
    ESP_LOGI("wordcard_scan","CADENCE intervals=%u start_to_start_min=%lld avg=%lld max=%lld us (software LCD starts)",
        cadence_n,(long long)(cadence_n?cadence_min:0),
        (long long)(cadence_n?cadence_sum/cadence_n:0),(long long)cadence_max);
    ESP_LOGI("wordcard_scan","FEED first_empty=%d phase=%d thread=%d assigned=%d q=%d/%d other=%d/%d render_max_cycles=%lu/%lu rendered=%u/%u wait_timeout=%d/%d",
        feed_diag.first_line,feed_diag.phase,feed_diag.thread,feed_diag.assigned,
        feed_diag.current,feed_diag.last,feed_diag.other_current,feed_diag.other_last,
        (unsigned long)feed_diag.max_cycles[0],(unsigned long)feed_diag.max_cycles[1],
        feed_diag.rendered[0],feed_diag.rendered[1],feed_diag.wait_timeout_row[0],feed_diag.wait_timeout_row[1]);
}

__attribute__((optimize("O3"))) static bool IRAM_ATTR
push_pixels_isr(RenderContext_t* ctx, uint8_t* buf) {
    // Output no-op outside of drawn area
    if (ctx->lines_consumed < ctx->area.y) {
        memset(buf, 0, ctx->display_width / 4);
    } else if (ctx->lines_consumed >= ctx->area.y + ctx->area.height) {
        memset(buf, 0, ctx->display_width / 4);
    } else {
        memcpy(buf, ctx->static_line_buffer, ctx->display_width / 4);
    }
    ctx->lines_consumed += 1;
    return pdFALSE;
}

/**
 * Populate the line mask for use in epd_push_pixels.
 */
static void push_pixels_populate_line(RenderContext_t* ctx, int color) {
    // Select fill pattern by draw color
    int fill_byte = 0;
    switch (color) {
        case 0:
            fill_byte = DARK_BYTE;
            break;
        case 1:
            fill_byte = CLEAR_BYTE;
            break;
        default:
            fill_byte = 0x00;
    }

    // Compute a line mask based on the drawn area
    uint8_t* dirtyness = malloc(ctx->display_width / 2);
    assert(dirtyness != NULL);

    memset(dirtyness, 0, ctx->display_width / 2);

    for (int i = 0; i < ctx->display_width; i++) {
        if ((i >= ctx->area.x) && (i < ctx->area.x + ctx->area.width)) {
            dirtyness[i / 2] |= i % 2 ? 0xF0 : 0x0F;
        }
    }
    epd_populate_line_mask(ctx->line_mask, dirtyness, ctx->display_width / 4);

    // mask the line pattern with the populated mask
    memset(ctx->static_line_buffer, fill_byte, ctx->display_width / 4);
    epd_apply_line_mask(ctx->static_line_buffer, ctx->line_mask, ctx->display_width / 4);

    free(dirtyness);
}

void epd_push_pixels_lcd(RenderContext_t* ctx, short time, int color) {
    ctx->current_frame = 0;
    ctx->lines_total = ctx->display_height;
    ctx->lines_consumed = 0;
    ctx->static_line_buffer = malloc(ctx->display_width / 4);
    assert(ctx->static_line_buffer != NULL);

    push_pixels_populate_line(ctx, color);
    epd_lcd_frame_done_cb((frame_done_func_t)handle_lcd_frame_done, ctx);
    epd_lcd_line_source_cb((line_cb_func_t)&push_pixels_isr, ctx);

    epd_set_mode(1);
    epd_lcd_start_frame();
    xSemaphoreTake(ctx->frame_done, portMAX_DELAY);
    epd_set_mode(0);

    free(ctx->static_line_buffer);
    ctx->static_line_buffer = NULL;
}

#define int_min(a, b) (((a) < (b)) ? (a) : (b))

// 预填够 bounce 再开扫；太少易欠载，太多则短队列在开扫前会堵死
static int s_prefill_lines = 127;

__attribute__((optimize("O3"))) void IRAM_ATTR
lcd_calculate_frame(RenderContext_t* ctx, int thread_id) {
    assert(ctx->lut_lookup_func != NULL);
    uint8_t* input_line = ctx->feed_line_buffers[thread_id];

    LineQueue_t* lq = &ctx->line_queues[thread_id];
    int l = 0;

    // 另一生产者可能已启动并报告欠载；不得再次启动同一帧
    if (ctx->error) {
        return;
    }

    // line must be able to hold 2-pixel-per-byte or 1-pixel-per-byte data
    memset(input_line, 0x00, ctx->display_width);

    EpdRect area = ctx->area;
    int min_y, max_y, bytes_per_line, _ppB;
    const uint8_t* ptr_start;
    get_buffer_params(ctx, &bytes_per_line, &ptr_start, &min_y, &max_y, &_ppB);

    assert(area.width == ctx->display_width && area.x == 0 && !ctx->error);

    const int trigger_line = int_min(s_prefill_lines, max_y - min_y);

    while (l = atomic_fetch_add(&ctx->lines_prepared, 1), l < ctx->lines_total) {
        ctx->line_threads[l] = (uint8_t)thread_id;

        // 预填够行后开扫，随后边扫边喂
        if (l - min_y == trigger_line) {
            epd_lcd_line_source_cb((line_cb_func_t)&retrieve_line_isr, ctx);
            epd_lcd_start_frame();
        }

        if (l < min_y || l >= max_y
            || (ctx->drawn_lines != NULL && !ctx->drawn_lines[l - area.y])) {
            uint8_t* buf = NULL;
            int wait_iters = 0;
            while (buf == NULL) {
                if (ctx->error & EPD_DRAW_EMPTY_LINE_QUEUE) {
                    lq_reset(lq);
                    return;
                }
                buf = lq_current(lq);
                if (buf != NULL) {
                    break;
                }
                vTaskDelay(0);
                if (++wait_iters > 30000) {
                    feed_diag.wait_timeout_row[thread_id]=l;
                    ctx->error |= EPD_DRAW_EMPTY_LINE_QUEUE;
                    lq_reset(lq);
                    return;
                }
            }
            memset(buf, 0x00, lq->element_size);
            lq_commit(lq);
            continue;
        }

        const uint8_t* ptr = ptr_start + bytes_per_line * (l - min_y);
#if !CONFIG_IDF_TARGET_ESP32S31
        Cache_Start_DCache_Preload((uint32_t)ptr, ctx->display_width, 0);
#endif
        uint32_t* lp = (uint32_t*)ptr;

        uint8_t* buf = NULL;
        int wait_iters = 0;
        while (buf == NULL) {
            if (ctx->error & EPD_DRAW_EMPTY_LINE_QUEUE) {
                lq_reset(lq);
                return;
            }
            buf = lq_current(lq);
            if (buf != NULL) {
                break;
            }
            vTaskDelay(0);
            if (++wait_iters > 30000) {
                feed_diag.wait_timeout_row[thread_id]=l;
                ctx->error |= EPD_DRAW_EMPTY_LINE_QUEUE;
                lq_reset(lq);
                return;
            }
        }

        const uint32_t render_start=esp_cpu_get_cycle_count();
        // Convert every pixel once using the appropriate prebuilt LUT. Both
        // types use the existing optimized aligned converter for their spans.
        if(!wordcard_quality_render(l,ctx->current_frame,ptr,buf,ctx->conversion_lut,ctx->lut_lookup_func))
            ctx->lut_lookup_func(lp, buf, ctx->conversion_lut, ctx->display_width);
        epd_apply_line_mask_VE(buf, ctx->line_mask, ctx->display_width / 4);
        const uint32_t render_cycles=esp_cpu_get_cycle_count()-render_start;
        if(render_cycles>feed_diag.max_cycles[thread_id])feed_diag.max_cycles[thread_id]=render_cycles;
        ++feed_diag.rendered[thread_id];
        lq_commit(lq);
    }
}

#endif
