#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(_WIN32)
#include <direct.h>
#endif

#include "tests/support/rt_blackbox.h"
#include "tests/support/rt_queues.h"
#include "tests/support/rt_timing.h"
#include "tests/support/seq_rt_runs.h"

#define RT_TRACK_COUNT 16

#if !defined(ARRAY_SIZE)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

typedef struct {
    double p99_ns;
    unsigned silent_ticks;
    unsigned unmatched_on;
    unsigned unmatched_off;
    uint32_t max_len_ticks;
    uint32_t event_queue_hwm;
    uint32_t player_queue_hwm;
    unsigned track_on[RT_TRACK_COUNT];
    unsigned track_off[RT_TRACK_COUNT];
} rt_metrics_t;

static void capture_metrics(rt_metrics_t *out) {
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->p99_ns = rt_tim_p99_ns();
    out->silent_ticks = bb_silent_ticks();
    out->unmatched_on = bb_unmatched_on();
    out->unmatched_off = bb_unmatched_off();
    out->max_len_ticks = bb_max_note_len_ticks();
    out->event_queue_hwm = rq_event_high_watermark();
    out->player_queue_hwm = rq_player_high_watermark();

    for (unsigned track = 0; track < ARRAY_SIZE(out->track_on); ++track) {
        out->track_on[track] = bb_track_on_count((uint8_t)track);
        out->track_off[track] = bb_track_off_count((uint8_t)track);
    }
}

static int ensure_out_directory(void) {
#if defined(_WIN32)
    if (_mkdir("out") != 0) {
        if (errno != EEXIST) {
            return -1;
        }
    }
#else
    if (mkdir("out", 0777) != 0) {
        if (errno != EEXIST) {
            return -1;
        }
    }
#endif
    return 0;
}

static int write_report(const rt_metrics_t *stress,
                        const rt_metrics_t *soak) {
    if (ensure_out_directory() != 0) {
        perror("mkdir out");
        return -1;
    }

    FILE *f = fopen("out/host_rt_report.txt", "w");
    if (f == NULL) {
        perror("open report");
        return -1;
    }

    fprintf(f, "== Host RT Report ==\n");
    if (stress != NULL) {
        fprintf(f, "[stress]\n");
        fprintf(f, "p99_tick_ns=%.0f\n", stress->p99_ns);
        fprintf(f, "silent_ticks=%u\n", stress->silent_ticks);
        fprintf(f, "unmatched_on=%u\n", stress->unmatched_on);
        fprintf(f, "unmatched_off=%u\n", stress->unmatched_off);
        fprintf(f, "max_len_ticks=%lu\n", (unsigned long)stress->max_len_ticks);
        fprintf(f, "event_queue_hwm=%u\n", (unsigned)stress->event_queue_hwm);
        fprintf(f, "player_queue_hwm=%u\n", (unsigned)stress->player_queue_hwm);
        for (unsigned track = 0; track < ARRAY_SIZE(stress->track_on); ++track) {
            fprintf(f,
                    "track%02u_on=%u track%02u_off=%u\n",
                    track,
                    stress->track_on[track],
                    track,
                    stress->track_off[track]);
        }
    }

    if (soak != NULL) {
        fprintf(f, "[soak]\n");
        fprintf(f, "p99_tick_ns=%.0f\n", soak->p99_ns);
        fprintf(f, "silent_ticks=%u\n", soak->silent_ticks);
        fprintf(f, "unmatched_on=%u\n", soak->unmatched_on);
        fprintf(f, "unmatched_off=%u\n", soak->unmatched_off);
        fprintf(f, "max_len_ticks=%lu\n", (unsigned long)soak->max_len_ticks);
        fprintf(f, "event_queue_hwm=%u\n", (unsigned)soak->event_queue_hwm);
        fprintf(f, "player_queue_hwm=%u\n", (unsigned)soak->player_queue_hwm);
        for (unsigned track = 0; track < ARRAY_SIZE(soak->track_on); ++track) {
            fprintf(f,
                    "track%02u_on=%u track%02u_off=%u\n",
                    track,
                    soak->track_on[track],
                    track,
                    soak->track_off[track]);
        }
    }

    fclose(f);
    return 0;
}

static int check_core_guards(const rt_metrics_t *metrics) {
    if (metrics == NULL) {
        return 0;
    }

    if ((metrics->silent_ticks != 0U) || (metrics->unmatched_on != 0U) || (metrics->unmatched_off != 0U)) {
        return -1;
    }

    return 0;
}

int main(void) {
    rt_metrics_t stress_metrics;
    rt_metrics_t soak_metrics;
    int rc;

    rc = seq_rt_run_16tracks_stress();
    if (rc != EXIT_SUCCESS) {
        return rc;
    }
    capture_metrics(&stress_metrics);
    if (check_core_guards(&stress_metrics) != 0) {
        return EXIT_FAILURE;
    }

    rc = seq_rt_run_16tracks_soak();
    if (rc != EXIT_SUCCESS) {
        return rc;
    }
    capture_metrics(&soak_metrics);
    if (check_core_guards(&soak_metrics) != 0) {
        return EXIT_FAILURE;
    }

    if (write_report(&stress_metrics, &soak_metrics) != 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
